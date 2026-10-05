package dev.craftv.client;

import dev.craftv.CraftLog;
import net.minecraft.client.Minecraft;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.HitResult;

/**
 * The stand-in friend's autopilot ({@code -Dcraftv.friendBot=true}, tests only): walks a wide circle,
 * jumps now and then, and every few seconds places a stone block on the ground ahead and breaks it again,
 * using the same inputs a player would (keys and the game mode's use/attack). It gives automated tests,
 * and later phases' in-game checks, a friend that moves and builds without a second person.
 */
public final class FriendBot {
	public static final boolean ENABLED = MirrorWorld.FRIEND && Boolean.getBoolean("craftv.friendBot");
	private static final float TURN_DEG_PER_TICK = 1.0F; // ~12-block circles at walking speed
	private static final int JUMP_EVERY = 60, JUMP_TICKS = 2;
	private static final int BUILD_EVERY = 100, BREAK_AFTER = 40;
	private static final float LOOK_DOWN = 60.0F;
	private static final int HOTBAR_MENU_SLOT = 36; // the inventory menu's first hotbar slot

	private static int ticks;
	private static BlockPos placed;

	private FriendBot() {
	}

	public static void tick(Minecraft minecraft) {
		LocalPlayer p = minecraft.player;
		if (!ENABLED || p == null || minecraft.level == null || minecraft.gameMode == null) {
			return;
		}
		minecraft.options.pauseOnLostFocus = false; // keep playing in the background
		if (minecraft.gui.screen() != null) {
			minecraft.gui.setScreen(null);
		}
		ticks++;
		minecraft.options.keyUp.setDown(true);
		minecraft.options.keyJump.setDown(ticks % JUMP_EVERY < JUMP_TICKS);
		p.setYRot(p.getYRot() + TURN_DEG_PER_TICK);
		int phase = ticks % BUILD_EVERY;
		if (phase == 0) {
			int slot = p.getInventory().getSelectedSlot();
			minecraft.gameMode.handleCreativeModeItemAdd(new ItemStack(Items.STONE), HOTBAR_MENU_SLOT + slot);
			p.setXRot(LOOK_DOWN);
		} else if (phase == 2) {
			if (minecraft.hitResult instanceof BlockHitResult hit && hit.getType() == HitResult.Type.BLOCK) {
				minecraft.gameMode.useItemOn(p, InteractionHand.MAIN_HAND, hit);
				placed = hit.getBlockPos().relative(hit.getDirection());
				CraftLog.info("friend bot: placed stone at " + placed.toShortString());
			}
			p.setXRot(0.0F);
		} else if (phase == BREAK_AFTER && placed != null) {
			minecraft.gameMode.startDestroyBlock(placed, Direction.UP);
			CraftLog.info("friend bot: broke " + placed.toShortString());
			placed = null;
		}
	}
}
