package dev.craftv.client;

import dev.craftv.CraftLog;
import dev.craftv.LinkService;
import dev.craftv.client.passthrough.HostCamera;
import dev.craftv.client.passthrough.PassthroughClient;
import dev.craftv.coop.CoopServer;
import dev.craftv.terrain.TerrainIndex;
import dev.craftv.link.Messages;
import dev.craftv.link.Proto;
import java.util.concurrent.atomic.AtomicInteger;
import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.event.lifecycle.v1.ClientTickEvents;
import net.fabricmc.fabric.api.client.rendering.v1.hud.HudElementRegistry;
import net.fabricmc.fabric.api.event.player.AttackBlockCallback;
import net.fabricmc.fabric.api.event.player.UseBlockCallback;
import net.minecraft.client.Minecraft;
import net.minecraft.resources.Identifier;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.item.BlockItem;
import net.minecraft.world.level.block.Block;

public final class CraftVClient implements ClientModInitializer {
	private static final boolean SHOW_HUD = Boolean.parseBoolean(System.getProperty("craftv.hud", "true"));
	private static final int HUD_X = 4, HUD_Y = 4, HUD_LINE = 10;
	private static final int COLOR_OK = 0xFF55FF55, COLOR_WAIT = 0xFFFFFF55, COLOR_BAD = 0xFFFF5555, COLOR_INFO = 0xFFFFFFFF;
	private static final AtomicInteger REQUEST_IDS = new AtomicInteger();

	@Override
	public void onInitializeClient() {
		if (MirrorWorld.FRIEND) {
			// A stand-in friend for automated co-op tests: plays like anyone joining from Multiplayer.
			ClientTickEvents.END_CLIENT_TICK.register(minecraft -> {
				MirrorWorld.tick(minecraft);
				FriendBot.tick(minecraft);
			});
			CraftLog.info("client initialised as a stand-in friend (no link)" + (FriendBot.ENABLED ? " with the autopilot on" : ""));
			return;
		}
		LinkService.get().start();
		// A rebuilt terrain chunk may not change a single block (same ground as before): re-mesh it so the owner's view
		// hides it (RenderSectionRegionMixin reads the index while meshing).
		TerrainIndex.setOnPut((key, columns) -> {
			int[] range = columns.yRange();
			if (range == null) {
				return;
			}
			int cx = (int) (long) key, cz = (int) (key >>> 32);
			Minecraft minecraft = Minecraft.getInstance();
			minecraft.execute(() -> {
				if (minecraft.level != null) {
					for (int sy = range[0] >> 4; sy <= range[1] >> 4; sy++) {
						minecraft.level.setSectionDirtyWithNeighbors(cx, sy, cz);
					}
				}
			});
		});
		ClientTickEvents.START_CLIENT_TICK.register(minecraft -> {
			try {
				PassthroughClient.startTick(minecraft);
			} catch (RuntimeException e) {
				CraftLog.error("passthrough tick failed", e);
			}
		});
		ClientTickEvents.END_CLIENT_TICK.register(minecraft -> {
			try {
				MirrorWorld.tick(minecraft);
				PlayerPuppet.tick(minecraft);
				PassthroughClient.endTick(minecraft);
				if (minecraft.player != null) {
					CoopServer.publishIfNeeded(minecraft.getSingleplayerServer()); // vanilla's "Open to LAN" runs here too
				}
			} catch (RuntimeException e) {
				CraftLog.error("client tick failed", e);
			}
		});
		registerIntents();
		if (SHOW_HUD) {
			HudElementRegistry.addLast(Identifier.fromNamespaceAndPath("craftv", "link_status"), (graphics, delta) -> {
				if (HostCamera.frame() != null) {
					return; // the passthrough: this would be drawn over the host's picture
				}
				Minecraft minecraft = Minecraft.getInstance();
				LinkService link = LinkService.get();
				int color = switch (link.state()) {
					case CONNECTED -> COLOR_OK;
					case ATTACHED, WAITING -> COLOR_WAIT;
					default -> COLOR_BAD;
				};
				graphics.text(minecraft.font, "CraftV " + link.statusLine(), HUD_X, HUD_Y, color, true);
				graphics.text(minecraft.font, CoopServer.hudLine(), HUD_X, HUD_Y + HUD_LINE, COLOR_INFO, true);
			});
		}
		CraftLog.info("client initialised");
	}

	/**
	 * The local player's own break/place actions are reported to the host as intents
	 * (PROTOCOL.md §7.5, M -> H). The resulting world change arrives separately as BLOCK_SET.
	 */
	private static void registerIntents() {
		AttackBlockCallback.EVENT.register((player, level, hand, pos, direction) -> {
			if (level.isClientSide()) {
				int id = REQUEST_IDS.incrementAndGet();
				LinkService.get().send(new Messages.BlockBreakRequest(id, pos.getX(), pos.getY(), pos.getZ(), direction.get3DDataValue(), 0));
				CraftLog.limited("intent-break", 250, "intent: break #" + id + " at " + pos.toShortString() + " face " + direction);
			}
			return InteractionResult.PASS;
		});
		UseBlockCallback.EVENT.register((player, level, hand, hit) -> {
			if (level.isClientSide() && player.getItemInHand(hand).getItem() instanceof BlockItem item) {
				int id = REQUEST_IDS.incrementAndGet();
				var pos = hit.getBlockPos();
				int blockId = Block.getId(item.getBlock().defaultBlockState());
				LinkService.get().send(new Messages.BlockPlaceRequest(id, pos.getX(), pos.getY(), pos.getZ(), hit.getDirection().get3DDataValue(), blockId));
				CraftLog.limited("intent-place", 250, "intent: place #" + id + " against " + pos.toShortString() + " face " + hit.getDirection());
			}
			return InteractionResult.PASS;
		});
		CraftLog.debug("intents registered (break/place requests M->H use face ordinals 0.." + Proto.FACE_MAX + ")");
	}
}
