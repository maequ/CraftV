package dev.craftv.client;

import dev.craftv.LinkService;
import dev.craftv.CraftLog;
import dev.craftv.link.Messages;
import java.util.Set;
import java.util.UUID;
import net.minecraft.client.Minecraft;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.client.server.IntegratedServer;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.phys.Vec3;

/**
 * Phase 1/2 "puppet" mode (PROTOCOL.md §7.3): the host's PLAYER_STATE is where the Minecraft
 * player is. Applied at the end of every client tick: the player's xo/yo/zo were set to the
 * previous applied position at the start of the tick, so Minecraft's renderer interpolates
 * smoothly between them (one tick of latency). Large jumps, TELEPORT and a new host snap instead.
 * When the host stops sending (mock {@code stop}, a paused game), the player is handed back to the
 * mouse and keyboard until a new state arrives.
 */
public final class PlayerPuppet {
	private static final double SNAP_DISTANCE = 16.0;
	private static final float TICKS_PER_SECOND = 20.0F;
	private static final long HOST_PAUSE_NS = 250_000_000L;

	private static int lastGeneration = -1;
	private static LocalPlayer lastPlayer;
	private static long applied;
	private static Messages.PlayerState lastState;
	private static long lastNewStateNs;
	/** Where a player who can't fly is held while the host isn't driving them (no state yet, or paused). */
	private static Vec3 held;
	private static LocalPlayer heldPlayer;

	private PlayerPuppet() {
	}

	/**
	 * A jump must also move the player on the integrated server. The client only sends its position while the
	 * chunk it stands in is loaded, and the server only loads chunks around where it thinks the player is, so
	 * after a long jump (the host's first position, fast travel) neither side would ever move.
	 */
	private static void moveOnServer(Minecraft minecraft, LocalPlayer player, Messages.PlayerState ps) {
		IntegratedServer server = minecraft.getSingleplayerServer();
		if (server == null) {
			return;
		}
		UUID id = player.getUUID();
		server.execute(() -> {
			ServerPlayer owner = server.getPlayerList().getPlayer(id);
			if (owner != null) {
				owner.teleportTo(owner.level(), ps.x(), ps.y(), ps.z(), Set.of(), ps.yaw(), ps.pitch(), false);
			}
		});
	}

	/**
	 * A Creative owner gets the mouse and keyboard back when the host stops; a Survival owner would fall (into the
	 * void, where no ground is built yet), so they stay where the host last put them.
	 */
	private static void hold(LocalPlayer player) {
		if (player.getAbilities().mayfly) {
			held = null;
			return;
		}
		if (held == null || player != heldPlayer) {
			held = player.position();
			heldPlayer = player;
		}
		player.setPos(held.x, held.y, held.z);
		player.setDeltaMovement(Vec3.ZERO);
		player.resetFallDistance();
	}

	public static void tick(Minecraft minecraft) {
		LinkService link = LinkService.get();
		LocalPlayer player = minecraft.player;
		link.setInGame(player != null);
		if (player == null) {
			lastPlayer = null;
			return;
		}
		Messages.PlayerState ps = link.latestPlayerState();
		if (ps == null) {
			lastState = null;
			hold(player);
			return;
		}
		// Every received record is a new object, so identity tells a fresh state from a repeat.
		long now = System.nanoTime();
		if (ps != lastState) {
			lastState = ps;
			lastNewStateNs = now;
		} else if (now - lastNewStateNs > HOST_PAUSE_NS) {
			hold(player);
			return;
		}
		// The host drives this window's player; pausing on focus loss would freeze it.
		minecraft.options.pauseOnLostFocus = false;

		boolean newHost = link.peerGeneration() != lastGeneration;
		boolean snap = ps.teleport() || newHost || player != lastPlayer || player.position().distanceToSqr(ps.x(), ps.y(), ps.z()) > SNAP_DISTANCE * SNAP_DISTANCE;
		lastGeneration = link.peerGeneration();
		lastPlayer = player;
		if (snap) {
			player.snapTo(ps.x(), ps.y(), ps.z(), ps.yaw(), ps.pitch()); // also resets the interpolation start
			moveOnServer(minecraft, player, ps);
			CraftLog.info(String.format("puppet: snapped to (%.2f, %.2f, %.2f)%s", ps.x(), ps.y(), ps.z(), newHost ? " for a new host session" : ""));
		} else {
			player.setPos(ps.x(), ps.y(), ps.z());
			player.setYRot(ps.yaw());
			player.setXRot(ps.pitch());
		}
		player.setYHeadRot(ps.yaw());
		player.setYBodyRot(ps.yaw());
		// Per-tick velocity: the next tick's physics and walk animation see the host's motion.
		player.setDeltaMovement(ps.vx() / TICKS_PER_SECOND, ps.vy() / TICKS_PER_SECOND, ps.vz() / TICKS_PER_SECOND);
		player.setOnGround(ps.onGround());
		player.resetFallDistance();
		held = null;
		if (++applied % 1200 == 1) {
			CraftLog.info(String.format("puppet: applied %d host states; now (%.2f, %.2f, %.2f) yaw %.1f", applied, ps.x(), ps.y(), ps.z(), ps.yaw()));
		}
	}
}
