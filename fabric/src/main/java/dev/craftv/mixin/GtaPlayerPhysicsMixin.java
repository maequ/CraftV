package dev.craftv.mixin;

import dev.craftv.coop.GuestSync;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.player.Player;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * A GTA player (the owner or a guest) stands where GTA says, which can be inside CraftV's terrain (under a bridge,
 * in a building GTA scanned from above). The server's move check would call that "moved wrongly" and pull them back
 * (javap on 26.3: handleMovePlayer accepts any move while {@code noPhysics}). Player.tick resets noPhysics to
 * isSpectator() every tick, so this sets it again after.
 */
@Mixin(Player.class)
abstract class GtaPlayerPhysicsMixin {
	@Inject(method = "tick", at = @At("TAIL"))
	private void craftv$gtaPlayerHasNoPhysics(CallbackInfo ci) {
		if ((Object) this instanceof ServerPlayer p && p.level().getServer() != null && GuestSync.isGtaPlayer(p.level().getServer(), p)) {
			p.noPhysics = true;
		}
	}
}
