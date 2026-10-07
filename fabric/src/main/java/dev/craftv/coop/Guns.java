package dev.craftv.coop;

import dev.craftv.link.Proto;
import net.minecraft.core.component.DataComponents;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.component.CustomData;

/**
 * CraftV's gun items: Minecraft items that stand for GTA's guns. Holding one gives the GTA player that gun with GTA's
 * own aim and fire (PROTOCOL.md §7.18, held 16+). They're trial keys underneath (no recipes use them, and right
 * click does nothing outside a vault) with CraftV's own look, name and a tag saying which gun. The order here is the
 * host's gun table (gta_game.cpp kGuns).
 */
public final class Guns {
	public static final String[] IDS = { "pistol", "smg", "assault_rifle", "shotgun", "sniper_rifle", "rpg", "minigun", "grenade" };
	public static final String[] NAMES = { "Pistol", "SMG", "Assault Rifle", "Shotgun", "Sniper Rifle", "RPG", "Minigun", "Grenade" };
	private static final String TAG = "craftv_gun";

	static {
		if (IDS.length != Proto.HELD_GUN_COUNT) {
			throw new IllegalStateException("the gun table must have " + Proto.HELD_GUN_COUNT + " guns");
		}
	}

	private Guns() {
	}

	public static ItemStack stack(int index) {
		ItemStack s = new ItemStack(Items.TRIAL_KEY);
		s.set(DataComponents.ITEM_NAME, Component.literal(NAMES[index]));
		s.set(DataComponents.ITEM_MODEL, Identifier.fromNamespaceAndPath("craftv", IDS[index]));
		s.set(DataComponents.MAX_STACK_SIZE, 1);
		CompoundTag tag = new CompoundTag();
		tag.putInt(TAG, index);
		s.set(DataComponents.CUSTOM_DATA, CustomData.of(tag));
		return s;
	}

	/** Which gun a stack is, or -1. */
	public static int index(ItemStack stack) {
		if (stack.isEmpty() || !stack.is(Items.TRIAL_KEY)) {
			return -1;
		}
		CustomData data = stack.get(DataComponents.CUSTOM_DATA);
		int i = data == null ? -1 : data.copyTag().getIntOr(TAG, -1);
		return i >= 0 && i < IDS.length ? i : -1;
	}
}
