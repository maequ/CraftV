package dev.redcraft.client;

import dev.redcraft.LinkService;
import dev.redcraft.RedLog;
import dev.redcraft.link.Messages;
import net.minecraft.client.Minecraft;
import net.minecraft.client.player.LocalPlayer;

/**
 * Phase 1/2 "puppet" mode (PROTOCOL.md §7.3): the host's PLAYER_STATE is where the Minecraft
 * player is. Applied at the end of every client tick: the player's xo/yo/zo were set to the
 * previous applied position at the start of the tick, so Minecraft's renderer interpolates
 * smoothly between them (one tick of latency). Large jumps, TELEPORT and a new host snap instead.
 */
public final class PlayerPuppet {
	private static final double SNAP_DISTANCE = 16.0;
	private static final float TICKS_PER_SECOND = 20.0F;

	private static int lastGeneration = -1;
	private static LocalPlayer lastPlayer;
	private static long applied;

	private PlayerPuppet() {
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
			RedLog.info(String.format("puppet: snapped to (%.2f, %.2f, %.2f)%s", ps.x(), ps.y(), ps.z(), newHost ? " for a new host session" : ""));
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
		if (++applied % 1200 == 1) {
			RedLog.info(String.format("puppet: applied %d host states; now (%.2f, %.2f, %.2f) yaw %.1f", applied, ps.x(), ps.y(), ps.z(), ps.yaw()));
		}
	}
}
