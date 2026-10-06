package dev.craftv.client.passthrough;

import static dev.craftv.link.Proto.*;

import dev.craftv.LinkService;
import dev.craftv.client.mixin.KeyMappingAccessor;
import dev.craftv.link.Messages;
import net.minecraft.client.KeyMapping;
import net.minecraft.client.Minecraft;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.world.entity.player.Inventory;

/**
 * The owner's controls, forwarded by the host (PROTOCOL.md §7.17): the host window has the focus, so Minecraft
 * never sees these itself. Applied at the start of each client tick, before vanilla handles key bindings.
 * Adapted from minecraft-gta5-passthrough (rehan-remade, MIT).
 */
public final class ClientInput {
	private ClientInput() {
	}

	public static void tick(Minecraft minecraft) {
		Messages.Input in;
		while ((in = LinkService.get().pollInput()) != null) {
			handle(minecraft, in);
		}
	}

	private static void handle(Minecraft minecraft, Messages.Input in) {
		LocalPlayer player = minecraft.player;
		switch (in.kind()) {
			case INPUT_BUTTON -> {
				boolean down = in.down() == 1;
				if (in.button() == BUTTON_CLOSE_SCREEN) {
					if (down && minecraft.gui.screen() != null) {
						minecraft.gui.screen().onClose();
					}
					return;
				}
				KeyMapping key = switch (in.button()) {
					case BUTTON_ATTACK -> minecraft.options.keyAttack;
					case BUTTON_USE -> minecraft.options.keyUse;
					case BUTTON_PICK -> minecraft.options.keyPickItem;
					case BUTTON_DROP -> minecraft.options.keyDrop;
					case BUTTON_INVENTORY -> minecraft.options.keyInventory;
					case BUTTON_SWAP_HANDS -> minecraft.options.keySwapOffhand;
					default -> null;
				};
				if (key == null) {
					return;
				}
				if (down && !key.isDown()) {
					KeyMappingAccessor access = (KeyMappingAccessor) key;
					access.craftv$setClickCount(access.craftv$getClickCount() + 1);
				}
				key.setDown(down);
			}
			case INPUT_SLOT -> {
				if (player != null) {
					player.getInventory().setSelectedSlot(Math.clamp(in.value(), 0, Inventory.getSelectionSize() - 1));
				}
			}
			case INPUT_SCROLL -> {
				if (player != null) {
					Inventory inventory = player.getInventory();
					inventory.setSelectedSlot(Math.floorMod(inventory.getSelectedSlot() + in.value(), Inventory.getSelectionSize()));
				}
			}
			default -> {
			}
		}
	}
}
