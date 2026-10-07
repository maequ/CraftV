package dev.craftv.coop;

import static dev.craftv.link.Proto.*;

import dev.craftv.CraftLog;
import dev.craftv.LinkService;
import dev.craftv.link.Messages;
import dev.craftv.mixin.AbstractArrowAccessor;
import dev.craftv.net.CraftNet;
import dev.craftv.terrain.TerrainService;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import net.fabricmc.fabric.api.networking.v1.ServerPlayNetworking;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.projectile.arrow.AbstractArrow;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;

/**
 * What the Minecraft world does to the GTA players' games and back (protocol v1.4): explosions and flying arrows go
 * to every GTA player near them (PROTOCOL.md §7.20), GTA damage becomes Minecraft damage, and running in GTA costs
 * hunger as sprinting in Minecraft does. Server thread unless noted.
 */
public final class GtaWorld {
	/** What a GTA player is doing (set by their client: the owner's directly, a guest's with CraftNet.GtaState). */
	public static final int STATE_SPRINTING = 1, STATE_IN_VEHICLE = 1 << 1;
	/** Minecraft's own: sprinting costs 0.1 exhaustion per metre (4 exhaustion = one food point). */
	private static final float SPRINT_EXHAUSTION_PER_METRE = 0.1F;
	/** A move longer than this in one tick isn't walking (a snap, a vehicle the flags missed). */
	private static final double MAX_FOOT_STEP = 1.5;
	/** GTA players hear about explosions and arrows this close to them (blocks). */
	private static final double EVENT_RADIUS = 128.0;
	private static final int MAX_ARROWS_PER_TICK = 64;

	private static final Map<UUID, Integer> STATES = new ConcurrentHashMap<>();
	private static final Map<UUID, Vec3> LAST_POS = new ConcurrentHashMap<>();
	private static final List<Messages.WorldEvent> PENDING = new ArrayList<>();
	private static long explosionsSent, arrowsSent;

	private GtaWorld() {
	}

	static void init() {
		ServerPlayNetworking.registerGlobalReceiver(CraftNet.Hurt.TYPE, (p, ctx) -> {
			if (GuestSync.isGuest(ctx.player())) {
				hurt(ctx.player(), p.cause(), p.halfHearts());
			}
		});
		ServerPlayNetworking.registerGlobalReceiver(CraftNet.GtaState.TYPE, (p, ctx) -> {
			if (GuestSync.isGuest(ctx.player())) {
				STATES.put(ctx.player().getUUID(), p.flags());
			}
		});
	}

	/** The owner's client (same JVM), from any thread: what their GTA player is doing. */
	public static void setState(UUID player, int flags) {
		STATES.put(player, flags);
	}

	static void left(ServerPlayer player) {
		STATES.remove(player.getUUID());
		LAST_POS.remove(player.getUUID());
	}

	static void clear() {
		STATES.clear();
		LAST_POS.clear();
		PENDING.clear();
	}

	/**
	 * GTA damage (INPUT DAMAGE): Minecraft's hearts decide life and death in the passthrough. No invulnerability
	 * frames in between: GTA already spread the hits out.
	 */
	public static void hurt(ServerPlayer player, int cause, int halfHearts) {
		if (halfHearts <= 0 || !player.isAlive()) {
			return;
		}
		ServerLevel level = player.level();
		DamageSource source = switch (cause) {
			case DAMAGE_FALL -> level.damageSources().fall();
			case DAMAGE_EXPLOSION -> level.damageSources().explosion(null, null);
			case DAMAGE_FIRE -> level.damageSources().onFire();
			case DAMAGE_DROWN -> level.damageSources().drown();
			case DAMAGE_VEHICLE -> level.damageSources().flyIntoWall();
			default -> level.damageSources().generic();
		};
		player.setInvulnerableTime(0);
		player.hurtServer(level, source, halfHearts);
		CraftLog.limited("gtahurt", 1000, player.getGameProfile().name() + " got hurt in GTA: " + halfHearts + " half hearts (cause " + cause + ")");
	}

	/** From ServerExplosionMixin, when an explosion goes off in a server world. */
	public static void onExplosion(ServerLevel level, Vec3 center, float radius) {
		if (level == TerrainService.mirror(level.getServer())) {
			PENDING.add(new Messages.WorldEvent(EVENT_EXPLOSION, Math.min(radius, MAX_EVENT_POWER), center.x, center.y, center.z, 0));
		}
	}

	static void tick(MinecraftServer server) {
		ServerLevel level = TerrainService.mirror(server);
		List<ServerPlayer> gta = new ArrayList<>();
		for (ServerPlayer p : server.getPlayerList().getPlayers()) {
			if (p.level() == level && GuestSync.isGtaPlayer(server, p)) {
				gta.add(p);
				exhaust(p);
			}
		}
		if (gta.isEmpty()) {
			PENDING.clear();
			return;
		}
		List<Messages.WorldEvent> events = new ArrayList<>(PENDING);
		PENDING.clear();
		java.util.Set<Integer> seen = new java.util.HashSet<>();
		for (ServerPlayer p : gta) {
			AABB near = p.getBoundingBox().inflate(EVENT_RADIUS);
			for (AbstractArrow arrow : level.getEntitiesOfClass(AbstractArrow.class, near, a -> !((AbstractArrowAccessor) a).craftv$isInGround())) {
				if (seen.size() >= MAX_ARROWS_PER_TICK) {
					break;
				}
				if (!seen.add(arrow.getId())) {
					continue; // near two GTA players: one event, sent to both
				}
				double speed = arrow.getDeltaMovement().length();
				float damage = (float) Math.min(Math.ceil(speed * ((AbstractArrowAccessor) arrow).craftv$getBaseDamage()), MAX_EVENT_POWER);
				events.add(new Messages.WorldEvent(EVENT_PROJECTILE, damage, arrow.getX(), arrow.getY(), arrow.getZ(), arrow.getId()));
			}
		}
		if (events.isEmpty()) {
			return;
		}
		LinkService link = LinkService.get();
		for (ServerPlayer p : gta) {
			boolean owner = CoopServer.isOwner(server, p);
			for (Messages.WorldEvent e : events) {
				if (p.distanceToSqr(e.x(), e.y(), e.z()) > EVENT_RADIUS * EVENT_RADIUS) {
					continue;
				}
				if (owner) {
					link.send(e);
				} else if (ServerPlayNetworking.canSend(p, CraftNet.Event.TYPE)) {
					ServerPlayNetworking.send(p, new CraftNet.Event(e.kind(), e.power(), e.x(), e.y(), e.z(), e.id()));
				}
				if (e.kind() == EVENT_EXPLOSION) {
					explosionsSent++;
					CraftLog.info("explosion at " + Math.round(e.x()) + ", " + Math.round(e.y()) + ", " + Math.round(e.z()) + " sent to "
						+ p.getGameProfile().name() + "'s GTA (" + explosionsSent + " so far)");
				} else {
					arrowsSent++;
				}
			}
		}
	}

	/** Running in GTA costs hunger like sprinting in Minecraft (the position comes from GTA, so vanilla never counts it). */
	private static void exhaust(ServerPlayer p) {
		Vec3 now = p.position();
		Vec3 last = LAST_POS.put(p.getUUID(), now);
		int flags = STATES.getOrDefault(p.getUUID(), 0);
		boolean sprinting = (flags & STATE_SPRINTING) != 0 && (flags & STATE_IN_VEHICLE) == 0;
		if (p.isSprinting() != sprinting) {
			p.setSprinting(sprinting); // friends see it too
		}
		if (last == null || !sprinting || p.getAbilities().invulnerable) {
			return;
		}
		double dx = now.x - last.x, dz = now.z - last.z, step = Math.sqrt(dx * dx + dz * dz);
		if (step > 0.0 && step < MAX_FOOT_STEP) {
			p.causeFoodExhaustion((float) step * SPRINT_EXHAUSTION_PER_METRE);
		}
	}
}
