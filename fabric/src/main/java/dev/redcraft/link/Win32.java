package dev.redcraft.link;

import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_INT;
import static java.lang.foreign.ValueLayout.JAVA_LONG;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.StructLayout;
import java.lang.foreign.SymbolLookup;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.VarHandle;
import java.nio.charset.StandardCharsets;

/**
 * kernel32 calls through java.lang.foreign (the JVM needs --enable-native-access=ALL-UNNAMED).
 * Pattern from SkyCraft's SkyLink (MIT, chasmlol); adds CreateFileMappingW (Minecraft may start
 * first, PROTOCOL.md §5.1) and VirtualQuery (validate the real view size, §3.2).
 */
public final class Win32 {
	private Win32() {
	}

	public static final int PAGE_READWRITE = 0x04;
	public static final int FILE_MAP_ALL_ACCESS = 0xF001F;
	public static final int ERROR_ALREADY_EXISTS = 183;
	public static final int ERROR_ACCESS_DENIED = 5;
	private static final MemorySegment INVALID_HANDLE_VALUE = MemorySegment.ofAddress(-1L);
	// MEMORY_BASIC_INFORMATION (x64): RegionSize is the size_t at offset 24; the struct is 48 bytes.
	private static final long MBI_BYTES = 48;
	private static final long MBI_REGION_SIZE = 24;

	private static final StructLayout CALL_STATE = Linker.Option.captureStateLayout();
	private static final VarHandle LAST_ERROR = CALL_STATE.varHandle(MemoryLayout.PathElement.groupElement("GetLastError"));

	private static final MethodHandle CREATE_FILE_MAPPING;
	private static final MethodHandle MAP_VIEW_OF_FILE;
	private static final MethodHandle UNMAP_VIEW_OF_FILE;
	private static final MethodHandle CLOSE_HANDLE;
	private static final MethodHandle VIRTUAL_QUERY;
	private static final MethodHandle GET_TICK_COUNT64;
	private static final MethodHandle QUERY_PERFORMANCE_COUNTER;
	private static final MethodHandle QUERY_PERFORMANCE_FREQUENCY;
	private static final MethodHandle GET_CURRENT_PROCESS_ID;
	private static final MethodHandle CREATE_MUTEX;
	private static final long QPC_FREQUENCY;
	private static final long US_PER_SECOND = 1_000_000L;

	static {
		Linker linker = Linker.nativeLinker();
		SymbolLookup k32 = SymbolLookup.libraryLookup("kernel32", Arena.global());
		CREATE_FILE_MAPPING = linker.downcallHandle(k32.find("CreateFileMappingW").orElseThrow(),
			FunctionDescriptor.of(ADDRESS, ADDRESS, ADDRESS, JAVA_INT, JAVA_INT, JAVA_INT, ADDRESS), Linker.Option.captureCallState("GetLastError"));
		MAP_VIEW_OF_FILE = linker.downcallHandle(k32.find("MapViewOfFile").orElseThrow(),
			FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_INT, JAVA_INT, JAVA_INT, JAVA_LONG), Linker.Option.captureCallState("GetLastError"));
		UNMAP_VIEW_OF_FILE = linker.downcallHandle(k32.find("UnmapViewOfFile").orElseThrow(), FunctionDescriptor.of(JAVA_INT, ADDRESS));
		CLOSE_HANDLE = linker.downcallHandle(k32.find("CloseHandle").orElseThrow(), FunctionDescriptor.of(JAVA_INT, ADDRESS));
		VIRTUAL_QUERY = linker.downcallHandle(k32.find("VirtualQuery").orElseThrow(), FunctionDescriptor.of(JAVA_LONG, ADDRESS, ADDRESS, JAVA_LONG));
		GET_TICK_COUNT64 = linker.downcallHandle(k32.find("GetTickCount64").orElseThrow(), FunctionDescriptor.of(JAVA_LONG));
		QUERY_PERFORMANCE_COUNTER = linker.downcallHandle(k32.find("QueryPerformanceCounter").orElseThrow(), FunctionDescriptor.of(JAVA_INT, ADDRESS));
		QUERY_PERFORMANCE_FREQUENCY = linker.downcallHandle(k32.find("QueryPerformanceFrequency").orElseThrow(), FunctionDescriptor.of(JAVA_INT, ADDRESS));
		GET_CURRENT_PROCESS_ID = linker.downcallHandle(k32.find("GetCurrentProcessId").orElseThrow(), FunctionDescriptor.of(JAVA_INT));
		CREATE_MUTEX = linker.downcallHandle(k32.find("CreateMutexW").orElseThrow(), FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_INT, ADDRESS));
		try (Arena arena = Arena.ofConfined()) {
			MemorySegment out = arena.allocate(JAVA_LONG);
			int ok = (int) QUERY_PERFORMANCE_FREQUENCY.invokeExact(out);
			QPC_FREQUENCY = ok != 0 ? out.get(JAVA_LONG, 0) : 1;
		} catch (Throwable t) {
			throw new ExceptionInInitializerError(t);
		}
	}

	/** Result of {@link #createOrOpenMapping}: a mapped view or an error text. */
	public record MappedView(MemorySegment handle, MemorySegment base, long viewBytes, boolean created, String error) {
		public boolean ok() {
			return error == null;
		}
	}

	/** CreateFileMappingW + MapViewOfFile + VirtualQuery (PROTOCOL.md §5.1). */
	public static MappedView createOrOpenMapping(String name, long bytes) {
		try (Arena arena = Arena.ofConfined()) {
			MemorySegment state = arena.allocate(CALL_STATE);
			MemorySegment wname = arena.allocateFrom(name, StandardCharsets.UTF_16LE);
			MemorySegment handle = (MemorySegment) CREATE_FILE_MAPPING.invokeExact(state, INVALID_HANDLE_VALUE, MemorySegment.NULL, PAGE_READWRITE,
				(int) (bytes >>> 32), (int) bytes, wname);
			int createError = (int) LAST_ERROR.get(state, 0L);
			if (handle.address() == 0) {
				return new MappedView(null, null, 0, false, "CreateFileMappingW failed (Windows error " + createError
					+ (createError == ERROR_ACCESS_DENIED ? ": access denied; is the host running as administrator with an old build?" : "") + ")");
			}
			MemorySegment view = (MemorySegment) MAP_VIEW_OF_FILE.invokeExact(state, handle, FILE_MAP_ALL_ACCESS, 0, 0, 0L);
			if (view.address() == 0) {
				int err = (int) LAST_ERROR.get(state, 0L);
				closeHandle(handle);
				return new MappedView(null, null, 0, false, "MapViewOfFile failed (Windows error " + err + ")");
			}
			MemorySegment mbi = arena.allocate(MBI_BYTES, 8);
			long got = (long) VIRTUAL_QUERY.invokeExact(view, mbi, MBI_BYTES);
			if (got == 0) {
				unmap(view);
				closeHandle(handle);
				return new MappedView(null, null, 0, false, "VirtualQuery failed");
			}
			long viewBytes = mbi.get(JAVA_LONG, MBI_REGION_SIZE);
			return new MappedView(handle, view.reinterpret(viewBytes), viewBytes, createError != ERROR_ALREADY_EXISTS, null);
		} catch (Throwable t) {
			return new MappedView(null, null, 0, false, "mapping failed: " + t);
		}
	}

	public static void unmap(MemorySegment view) {
		try {
			int ignored = (int) UNMAP_VIEW_OF_FILE.invokeExact(view);
		} catch (Throwable ignored) {
			// nothing useful to do on shutdown
		}
	}

	public static void closeHandle(MemorySegment handle) {
		try {
			int ignored = (int) CLOSE_HANDLE.invokeExact(handle);
		} catch (Throwable ignored) {
			// nothing useful to do on shutdown
		}
	}

	/** GetTickCount64: the protocol's timeout clock. */
	public static long tickCount() {
		try {
			return (long) GET_TICK_COUNT64.invokeExact();
		} catch (Throwable t) {
			throw new IllegalStateException(t);
		}
	}

	private static final ThreadLocal<MemorySegment> QPC_OUT = ThreadLocal.withInitial(() -> Arena.global().allocate(JAVA_LONG));

	/** QueryPerformanceCounter in microseconds: the protocol's timestamp clock (same as the C++ side). */
	public static long nowUs() {
		try {
			MemorySegment out = QPC_OUT.get();
			int ok = (int) QUERY_PERFORMANCE_COUNTER.invokeExact(out);
			long counter = out.get(JAVA_LONG, 0);
			return (counter / QPC_FREQUENCY) * US_PER_SECOND + (counter % QPC_FREQUENCY) * US_PER_SECOND / QPC_FREQUENCY;
		} catch (Throwable t) {
			throw new IllegalStateException(t);
		}
	}

	public static int currentPid() {
		try {
			return (int) GET_CURRENT_PROCESS_ID.invokeExact();
		} catch (Throwable t) {
			throw new IllegalStateException(t);
		}
	}

	/** A named mutex held for the process lifetime, so the host can tell a Minecraft is running (SkyCraft's trick). */
	public static void holdNamedMutex(String name) {
		try (Arena arena = Arena.ofConfined()) {
			MemorySegment wname = arena.allocateFrom(name, StandardCharsets.UTF_16LE);
			MemorySegment ignored = (MemorySegment) CREATE_MUTEX.invokeExact(MemorySegment.NULL, 0, wname);
		} catch (Throwable ignored) {
			// optional feature
		}
	}
}
