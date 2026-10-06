package dev.craftv.client.passthrough;

import static dev.craftv.link.Proto.*;

import dev.craftv.CraftLog;
import dev.craftv.LinkService;
import dev.craftv.link.Messages;
import net.minecraft.client.CloudStatus;
import net.minecraft.client.InactivityFpsLimit;
import net.minecraft.client.Minecraft;
import net.minecraft.client.Options;
import net.minecraft.client.gui.screens.DeathScreen;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.client.tutorial.TutorialSteps;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.item.BlockItem;
import net.minecraft.world.item.ItemStack;
import org.lwjgl.sdl.SDLVideo;

/**
 * The client-thread side of the passthrough (brief §8): forwarded input, the window size the host wants, hiding
 * the terrain from the owner's view, respawning straight away (a death screen would sit over the host's picture),
 * and the owner's state for the host (PROTOCOL.md §7.18).
 */
public final class PassthroughClient {
	private static final int OWNER_STATE_EVERY_TICKS = 20;
	private static final int RESPAWN_DELAY_TICKS = 40;
	/** The terrain stays hidden this long after the last camera: a pause menu shouldn't re-mesh every chunk twice. */
	private static final long HIDE_TERRAIN_FOR_NANOS = 60_000_000_000L;

	private static boolean optionsConfigured;
	private static Messages.View appliedView;
	private static Messages.OwnerState lastOwner;
	private static int ownerTicks;
	private static int respawnIn = RESPAWN_DELAY_TICKS;

	private PassthroughClient() {
	}

	/** Start of every client tick: before vanilla handles key bindings, so forwarded clicks count this tick. */
	public static void startTick(Minecraft minecraft) {
		boolean on = HostCamera.live() != null;
		boolean hide = HostCamera.recent(HIDE_TERRAIN_FOR_NANOS);
		if (hide != Passthrough.hideTerrain()) {
			Passthrough.setHideTerrain(hide);
			if (minecraft.level != null) {
				// re-mesh every chunk with terrain hidden (or shown again)
				minecraft.levelRenderer.invalidateCompiledGeometry(minecraft.level, minecraft.options, minecraft.gameRenderer.mainCamera(),
					minecraft.getBlockColors());
			}
			CraftLog.info("passthrough " + (hide ? "on: rendering the owner's view for the host" : "off"));
			if (hide && !optionsConfigured) {
				optionsConfigured = true;
				configure(minecraft.options);
			}
			if (hide) {
				minecraft.gui.toastManager().clear(); // anything already showing (a tutorial hint) would sit over the host's picture
			}
		}
		if (on) {
			ClientInput.tick(minecraft);
			applyView(minecraft);
		}
		PlayerSync.tick();
	}

	/** End of every client tick. */
	public static void endTick(Minecraft minecraft) {
		LocalPlayer player = minecraft.player;
		if (player == null) {
			return;
		}
		if (minecraft.gui.screen() instanceof DeathScreen) {
			if (--respawnIn <= 0) {
				respawnIn = RESPAWN_DELAY_TICKS;
				player.respawn();
			}
		} else {
			respawnIn = RESPAWN_DELAY_TICKS;
		}
		sendOwnerState(minecraft, player);
	}

	/** Sitting behind the host's picture: keep running unfocused, no clouds, bobbing or screen effects. */
	private static void configure(Options options) {
		options.pauseOnLostFocus = false;
		options.tutorialStep = TutorialSteps.NONE;
		options.cloudStatus().set(CloudStatus.OFF);
		options.bobView().set(false);
		options.vignette().set(false);
		options.inactivityFpsLimit().set(InactivityFpsLimit.MINIMIZED);
		options.fovEffectScale().set(0.0);
		options.damageTiltStrength().set(0.0);
		options.menuBackgroundBlurriness().set(0);
		options.enableVsync().set(false);
		options.framerateLimit().set(120); // the host shows ~60-120 fps: rendering faster only competes with it for the GPU
		options.save();
		CraftLog.info("passthrough: options set for compositing (no clouds, no bobbing, 120 fps cap, runs unfocused)");
	}

	/** Minecraft's window = the host's picture (scaled down by the host to at most VIEW_MAX_PIXELS). */
	private static void applyView(Minecraft minecraft) {
		Messages.View v = LinkService.get().latestView();
		if (v == null || v.equals(appliedView)) {
			return;
		}
		appliedView = v;
		long handle = minecraft.getWindow().handle();
		SDLVideo.SDL_RestoreWindow(handle); // resizing a maximised window is ignored
		minecraft.getWindow().setWindowed(v.width(), v.height());
		SDLVideo.SDL_SetWindowSize(handle, v.width(), v.height());
		SDLVideo.SDL_SyncWindow(handle);
		CraftLog.info("passthrough: window sized to " + v.width() + "x" + v.height() + " for the host's " + v.hostWidth() + "x" + v.hostHeight());
	}

	private static void sendOwnerState(Minecraft minecraft, LocalPlayer player) {
		if (!LinkService.get().connected()) {
			return;
		}
		ItemStack held = player.getMainHandItem();
		int health = Mth.clamp(Mth.ceil(player.getHealth()), 0, 255);
		int food = Mth.clamp(player.getFoodData().getFoodLevel(), 0, MAX_FOOD);
		int mode = minecraft.gameMode != null ? Mth.clamp(minecraft.gameMode.getPlayerMode().getId(), 0, GAME_MODE_MAX) : 0;
		float damage = Mth.clamp((float) player.getAttributeValue(Attributes.ATTACK_DAMAGE), 0.0F, MAX_ATTACK_DAMAGE);
		float charge = Mth.clamp(player.getAttackStrengthScale(0.5F), 0.0F, 1.0F);
		int flags = (player.isDeadOrDying() ? OWNER_DEAD : 0) | (minecraft.gui.screen() != null ? OWNER_SCREEN_OPEN : 0);
		Messages.OwnerState s = new Messages.OwnerState(heldKind(held), health, food, mode, damage, charge, flags);
		// charge climbs every tick after a swing; it only matters to the host when it's full or nearly
		boolean changed = lastOwner == null || lastOwner.held() != s.held() || lastOwner.health() != s.health() || lastOwner.food() != s.food()
			|| lastOwner.gameMode() != s.gameMode() || lastOwner.flags() != s.flags() || lastOwner.attackDamage() != s.attackDamage()
			|| (lastOwner.attackCharge() >= 1.0F) != (s.attackCharge() >= 1.0F);
		if (changed || ++ownerTicks >= OWNER_STATE_EVERY_TICKS) {
			ownerTicks = 0;
			lastOwner = s;
			LinkService.get().send(s);
		}
	}

	static int heldKind(ItemStack stack) {
		if (stack.isEmpty()) {
			return HELD_EMPTY;
		}
		if (stack.getItem() instanceof BlockItem) {
			return HELD_BLOCK;
		}
		String path = BuiltInRegistries.ITEM.getKey(stack.getItem()).getPath();
		if (path.endsWith("_sword")) {
			return HELD_SWORD;
		}
		if (path.endsWith("_pickaxe")) {
			return HELD_PICKAXE;
		}
		if (path.endsWith("_axe")) {
			return HELD_AXE;
		}
		if (path.endsWith("_shovel")) {
			return HELD_SHOVEL;
		}
		if (path.endsWith("_hoe")) {
			return HELD_HOE;
		}
		return HELD_OTHER;
	}
}
