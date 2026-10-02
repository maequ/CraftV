package dev.redcraft.client;

import dev.redcraft.LinkService;
import dev.redcraft.RedLog;
import dev.redcraft.link.Messages;
import dev.redcraft.link.Proto;
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

public final class RedCraftClient implements ClientModInitializer {
	private static final boolean SHOW_HUD = Boolean.parseBoolean(System.getProperty("redcraft.hud", "true"));
	private static final int HUD_X = 4, HUD_Y = 4;
	private static final int COLOR_OK = 0xFF55FF55, COLOR_WAIT = 0xFFFFFF55, COLOR_BAD = 0xFFFF5555;
	private static final AtomicInteger REQUEST_IDS = new AtomicInteger();

	@Override
	public void onInitializeClient() {
		LinkService.get().start();
		ClientTickEvents.END_CLIENT_TICK.register(minecraft -> {
			try {
				DevWorld.tick(minecraft);
				PlayerPuppet.tick(minecraft);
			} catch (RuntimeException e) {
				RedLog.error("client tick failed", e);
			}
		});
		registerIntents();
		if (SHOW_HUD) {
			HudElementRegistry.addLast(Identifier.fromNamespaceAndPath("redcraft", "link_status"), (graphics, delta) -> {
				Minecraft minecraft = Minecraft.getInstance();
				LinkService link = LinkService.get();
				int color = switch (link.state()) {
					case CONNECTED -> COLOR_OK;
					case ATTACHED, WAITING -> COLOR_WAIT;
					default -> COLOR_BAD;
				};
				graphics.text(minecraft.font, "RedCraft " + link.statusLine(), HUD_X, HUD_Y, color, true);
			});
		}
		RedLog.info("client initialised");
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
				RedLog.limited("intent-break", 250, "intent: break #" + id + " at " + pos.toShortString() + " face " + direction);
			}
			return InteractionResult.PASS;
		});
		UseBlockCallback.EVENT.register((player, level, hand, hit) -> {
			if (level.isClientSide() && player.getItemInHand(hand).getItem() instanceof BlockItem item) {
				int id = REQUEST_IDS.incrementAndGet();
				var pos = hit.getBlockPos();
				int blockId = Block.getId(item.getBlock().defaultBlockState());
				LinkService.get().send(new Messages.BlockPlaceRequest(id, pos.getX(), pos.getY(), pos.getZ(), hit.getDirection().get3DDataValue(), blockId));
				RedLog.limited("intent-place", 250, "intent: place #" + id + " against " + pos.toShortString() + " face " + hit.getDirection());
			}
			return InteractionResult.PASS;
		});
		RedLog.debug("intents registered (break/place requests M->H use face ordinals 0.." + Proto.FACE_MAX + ")");
	}
}
