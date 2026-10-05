package dev.craftv.link;

import static dev.craftv.link.Proto.*;

import java.lang.foreign.MemorySegment;

/**
 * Single-producer / single-consumer byte ring over shared memory (PROTOCOL.md §4, §6). The Java
 * twin of protocol/cpp/include/craftv/ring.h; the algorithm comes from SkyCraft's rings (MIT,
 * chasmlol). Neither end ever blocks.
 */
public final class Ring {
	private Ring() {
	}

	/** Counters for one direction (both ends of a ring update their own). */
	public static final class Stats {
		public long produced, producedBytes, droppedFull;
		public long consumed, consumedBytes, stale, unknown, malformed, corrupt;
	}

	public enum PushResult { OK, FULL, TOO_LARGE }

	public enum DrainStatus { OK, CORRUPT }

	/** Receives each accepted record. {@code s} is the shared segment; the payload is at {@code payloadOff}. */
	@FunctionalInterface
	public interface Sink {
		void accept(int type, int typeVersion, int payloadBytes, int seq, MemorySegment s, long payloadOff);
	}

	/** The producing end. One thread only (PROTOCOL.md §4.2 "Produce"). */
	public static final class Producer {
		private final MemorySegment seg;
		private final long control;
		private final long data;
		private final long dataBytes;
		private long head;

		/** {@code control} and {@code data} are byte offsets into {@code seg}. Continues from the current head (§5.2). */
		public Producer(MemorySegment seg, long control, long data, long dataBytes) {
			this.seg = seg;
			this.control = control;
			this.data = data;
			this.dataBytes = dataBytes;
			this.head = (long) ATOMIC_LONG.getAcquire(seg, control + RC_HEAD);
		}

		public PushResult push(int type, int typeVersion, int session, int seq, Messages.Payload payload, Stats stats) {
			int payloadBytes = payload.payloadBytes();
			if (payloadBytes < 0 || payloadBytes > MAX_PAYLOAD) {
				return PushResult.TOO_LARGE;
			}
			long size = recordBytes(payloadBytes);
			long pos = head & (dataBytes - 1);
			long pad = pos + size > dataBytes ? dataBytes - pos : 0;
			long tail = (long) ATOMIC_LONG.getAcquire(seg, control + RC_TAIL);
			long used = head - tail;
			if (Long.compareUnsigned(used, dataBytes) > 0 || dataBytes - used < size + pad) {
				stats.droppedFull++;
				return PushResult.FULL;
			}
			if (pad > 0) {
				writeHeader(data + pos, MSG_PAD, 0, 0, session, 0);
				head += pad;
				pos = 0;
			}
			long at = data + pos;
			writeHeader(at, type, typeVersion, payloadBytes, session, seq);
			payload.write(seg, at + RECORD_HEADER_BYTES);
			long padding = size - RECORD_HEADER_BYTES - payloadBytes;
			if (padding > 0) {
				seg.asSlice(at + RECORD_HEADER_BYTES + payloadBytes, padding).fill((byte) 0);
			}
			head += size;
			ATOMIC_LONG.setRelease(seg, control + RC_HEAD, head);
			stats.produced++;
			stats.producedBytes += size;
			return PushResult.OK;
		}

		private void writeHeader(long at, int type, int typeVersion, int payloadBytes, int session, int seq) {
			seg.set(I16, at + RH_TYPE, (short) type);
			seg.set(I16, at + RH_TYPE_VERSION, (short) typeVersion);
			seg.set(I32, at + RH_PAYLOAD_BYTES, payloadBytes);
			seg.set(I32, at + RH_SESSION, session);
			seg.set(I32, at + RH_SEQ, seq);
		}

		public long backlog() {
			return head - (long) ATOMIC_LONG.getAcquire(seg, control + RC_TAIL);
		}
	}

	/** The consuming end. One thread only (PROTOCOL.md §4.2 "Consume"). */
	public static final class Consumer {
		private final MemorySegment seg;
		private final long control;
		private final long data;
		private final long dataBytes;

		public Consumer(MemorySegment seg, long control, long data, long dataBytes) {
			this.seg = seg;
			this.control = control;
			this.data = data;
			this.dataBytes = dataBytes;
		}

		/** Drops everything pending (§5.2 step 2, §4.3). */
		public void discardBacklog() {
			long head = (long) ATOMIC_LONG.getAcquire(seg, control + RC_HEAD);
			ATOMIC_LONG.setRelease(seg, control + RC_TAIL, head);
		}

		public long pending() {
			return (long) ATOMIC_LONG.getAcquire(seg, control + RC_HEAD) - (long) ATOMIC_LONG.getOpaque(seg, control + RC_TAIL);
		}

		/**
		 * Delivers each valid record of the producer's current session to {@code sink}, about
		 * {@code maxBytes} at most. {@code sessionOffset} is the producer's side-block session field;
		 * it is read (acquire) after head, as §4.2 requires.
		 */
		public DrainStatus drain(long sessionOffset, long maxBytes, Stats stats, Sink sink) {
			long head = (long) ATOMIC_LONG.getAcquire(seg, control + RC_HEAD);
			int session = (int) ATOMIC_INT.getAcquire(seg, sessionOffset);
			long tail = (long) ATOMIC_LONG.getOpaque(seg, control + RC_TAIL);
			long pending = head - tail;
			if (Long.compareUnsigned(pending, dataBytes) > 0 || (pending % RECORD_ALIGN) != 0 || (tail % RECORD_ALIGN) != 0) {
				return corrupt(head, stats);
			}
			long done = 0;
			while (Long.compareUnsigned(tail, head) < 0 && done < maxBytes) {
				long pos = tail & (dataBytes - 1);
				long toEnd = dataBytes - pos;
				if (toEnd < RECORD_HEADER_BYTES) {
					tail += toEnd;
					continue;
				}
				long at = data + pos;
				int type = Short.toUnsignedInt(seg.get(I16, at + RH_TYPE));
				if (type == MSG_PAD) {
					if (toEnd > head - tail) {
						return corrupt(head, stats);
					}
					tail += toEnd;
					continue;
				}
				int typeVersion = Short.toUnsignedInt(seg.get(I16, at + RH_TYPE_VERSION));
				int payloadBytes = seg.get(I32, at + RH_PAYLOAD_BYTES);
				if (payloadBytes < 0 || payloadBytes > MAX_PAYLOAD) {
					return corrupt(head, stats);
				}
				long size = recordBytes(payloadBytes);
				if (size > toEnd || size > head - tail) {
					return corrupt(head, stats);
				}
				int recordSession = seg.get(I32, at + RH_SESSION);
				int seq = seg.get(I32, at + RH_SEQ);
				if (recordSession != session) {
					stats.stale++;
				} else {
					stats.consumed++;
					stats.consumedBytes += size;
					sink.accept(type, typeVersion, payloadBytes, seq, seg, at + RECORD_HEADER_BYTES);
				}
				tail += size;
				done += size;
			}
			ATOMIC_LONG.setRelease(seg, control + RC_TAIL, tail);
			return DrainStatus.OK;
		}

		private DrainStatus corrupt(long head, Stats stats) {
			stats.corrupt++;
			ATOMIC_LONG.setRelease(seg, control + RC_TAIL, head);
			return DrainStatus.CORRUPT;
		}
	}
}
