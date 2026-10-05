package dev.craftv.link;

import static dev.craftv.link.Proto.*;

import java.lang.foreign.MemorySegment;

/**
 * Creator initialisation and reader validation of the mapping layout (PROTOCOL.md §3.2, §5.1).
 * Pure functions over a segment, so tests can feed them hand-made or corrupted buffers.
 */
public final class Layout {
	private Layout() {
	}

	/** Where one ring lives: byte offsets into the mapping. */
	public record RingLocation(long control, long data, long dataBytes) {
	}

	public record Resolved(RingLocation hostToMc, RingLocation mcToHost) {
	}

	/** Thrown (internally) with a human-readable reason. */
	public static final class Invalid extends Exception {
		public Invalid(String message) {
			super(message, null, false, false);
		}
	}

	/** Writes a fresh v1.0 header, section table and ring control pages, then the magic (release). Side blocks are left alone. */
	public static void initialize(MemorySegment s, int creatorRole, int creatorPid) {
		s.set(I16, H_VERSION_MAJOR, (short) VERSION_MAJOR);
		s.set(I16, H_VERSION_MINOR, (short) VERSION_MINOR);
		s.set(I32, H_HEADER_BYTES, (int) HEADER_BYTES);
		s.set(I32, H_SECTION_COUNT, SECTION_COUNT);
		s.set(I64, H_MAPPING_BYTES, MAPPING_BYTES);
		s.set(I32, H_CREATOR_ROLE, creatorRole);
		s.set(I32, H_CREATOR_PID, creatorPid);
		s.asSlice(OFF_SECTION_TABLE, SECTION_ENTRY_BYTES * MAX_SECTIONS).fill((byte) 0);
		writeSection(s, 0, SECTION_RING_HOST_TO_MC, OFF_RING_HOST_TO_MC);
		writeSection(s, 1, SECTION_RING_MC_TO_HOST, OFF_RING_MC_TO_HOST);
		writeRingControl(s, OFF_RING_HOST_TO_MC, ROLE_HOST);
		writeRingControl(s, OFF_RING_MC_TO_HOST, ROLE_MC);
		ATOMIC_INT.setRelease(s, H_MAGIC, MAGIC);
	}

	private static void writeSection(MemorySegment s, int index, int id, long offset) {
		long e = OFF_SECTION_TABLE + index * SECTION_ENTRY_BYTES;
		s.set(I32, e + SE_ID, id);
		s.set(I32, e + SE_FLAGS, 0);
		s.set(I64, e + SE_OFFSET, offset);
		s.set(I64, e + SE_BYTES, RING_SECTION_BYTES);
	}

	private static void writeRingControl(MemorySegment s, long offset, int producerRole) {
		s.asSlice(offset, RING_CONTROL_BYTES).fill((byte) 0);
		s.set(I64, offset + RC_DATA_BYTES, RING_DATA_BYTES);
		s.set(I32, offset + RC_PRODUCER_ROLE, producerRole);
	}

	/** Everything a reader checks before attaching (§3.2). {@code viewBytes} is the real view size. */
	public static Resolved validate(MemorySegment s, long viewBytes) throws Invalid {
		if (viewBytes < HEADER_BYTES || s.byteSize() < viewBytes) {
			throw new Invalid("view too small (" + viewBytes + " bytes)");
		}
		int magic = (int) ATOMIC_INT.getAcquire(s, H_MAGIC);
		if (magic != MAGIC) {
			throw new Invalid("bad magic 0x" + Integer.toHexString(magic));
		}
		int major = Short.toUnsignedInt(s.get(I16, H_VERSION_MAJOR));
		if (major != VERSION_MAJOR) {
			throw new Invalid("protocol major version " + major + ", expected " + VERSION_MAJOR);
		}
		int headerBytes = s.get(I32, H_HEADER_BYTES);
		if (headerBytes != HEADER_BYTES) {
			throw new Invalid("headerBytes 0x" + Integer.toHexString(headerBytes) + ", expected 0x" + Long.toHexString(HEADER_BYTES));
		}
		int sectionCount = s.get(I32, H_SECTION_COUNT);
		if (sectionCount < 1 || sectionCount > MAX_SECTIONS) {
			throw new Invalid("sectionCount " + sectionCount + " out of range");
		}
		long mappingBytes = s.get(I64, H_MAPPING_BYTES);
		if (Long.compareUnsigned(mappingBytes, viewBytes) > 0 || mappingBytes < HEADER_BYTES) {
			throw new Invalid("mappingBytes 0x" + Long.toHexString(mappingBytes) + " exceeds the view (0x" + Long.toHexString(viewBytes) + ")");
		}
		RingLocation a = ring(s, sectionCount, mappingBytes, SECTION_RING_HOST_TO_MC, ROLE_HOST);
		RingLocation b = ring(s, sectionCount, mappingBytes, SECTION_RING_MC_TO_HOST, ROLE_MC);
		long aEnd = a.data() + a.dataBytes(), bEnd = b.data() + b.dataBytes();
		if (a.control() < bEnd && b.control() < aEnd) {
			throw new Invalid("ring sections overlap");
		}
		return new Resolved(a, b);
	}

	private static RingLocation ring(MemorySegment s, int sectionCount, long mappingBytes, int id, int producerRole) throws Invalid {
		for (int i = 0; i < sectionCount; i++) {
			long e = OFF_SECTION_TABLE + i * SECTION_ENTRY_BYTES;
			if (s.get(I32, e + SE_ID) != id) {
				continue;
			}
			long offset = s.get(I64, e + SE_OFFSET);
			long bytes = s.get(I64, e + SE_BYTES);
			if (offset % SECTION_ALIGN != 0 || offset < HEADER_BYTES || Long.compareUnsigned(bytes, RING_CONTROL_BYTES) <= 0
				|| Long.compareUnsigned(offset, mappingBytes) > 0 || Long.compareUnsigned(bytes, mappingBytes - offset) > 0) {
				throw new Invalid("ring section " + id + " out of bounds (offset 0x" + Long.toHexString(offset) + ", bytes 0x" + Long.toHexString(bytes) + ")");
			}
			long dataBytes = bytes - RING_CONTROL_BYTES;
			if (Long.bitCount(dataBytes) != 1 || dataBytes < MIN_RING_DATA_BYTES || dataBytes > MAX_RING_DATA_BYTES) {
				throw new Invalid("ring section " + id + " data size 0x" + Long.toHexString(dataBytes) + " not a power of two in range");
			}
			if (s.get(I64, offset + RC_DATA_BYTES) != dataBytes || s.get(I32, offset + RC_PRODUCER_ROLE) != producerRole) {
				throw new Invalid("ring section " + id + " control page disagrees with the section table");
			}
			return new RingLocation(offset, offset + RING_CONTROL_BYTES, dataBytes);
		}
		throw new Invalid("ring section " + id + " missing");
	}
}
