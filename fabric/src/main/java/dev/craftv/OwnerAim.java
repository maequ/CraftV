package dev.craftv;

import java.util.UUID;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.phys.Vec3;

/**
 * Where the owner aims in third person: the host camera's ray (what is under the crosshair), set by the owner's client
 * every frame. The integrated server runs in the same process and reads it too, so buckets and bows, which aim from
 * the player's own eyes and rotation, aim at the crosshair instead (Sary, 2026-10-07: water didn't place, arrows went
 * somewhere else). Any thread.
 */
public final class OwnerAim {
	private static volatile Aim aim;

	public record Aim(UUID owner, Vec3 from, Vec3 dir) {
	}

	private OwnerAim() {
	}

	/** The owner's client, every frame in third person; null when first person or the passthrough is off. */
	public static void set(Aim a) {
		aim = a;
	}

	/** The camera ray for this player, or null (not the owner, first person, passthrough off). */
	public static Aim of(Player player) {
		Aim a = aim;
		return a != null && player.getUUID().equals(a.owner()) ? a : null;
	}
}
