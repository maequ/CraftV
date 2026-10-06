package dev.craftv.mixin;

import dev.craftv.coop.GuestSync;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.Entity;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * The owner goes where the host's player goes, and the terrain is GTA's ground probed from above: under a bridge or
 * inside a building the owner stands inside terrain blocks. That's not a wall in the host game, so it never
 * suffocates them (a Survival owner would die over and over otherwise).
 */
@Mixin(Entity.class)
abstract class EntityInWallMixin {
	@Inject(method = "isInWall", at = @At("HEAD"), cancellable = true)
	private void craftv$ownerNeverInWall(CallbackInfoReturnable<Boolean> cir) {
		if ((Object) this instanceof ServerPlayer player && player.level().getServer() != null && GuestSync.isGtaPlayer(player.level().getServer(), player)) {
			cir.setReturnValue(false);
		}
	}
}
