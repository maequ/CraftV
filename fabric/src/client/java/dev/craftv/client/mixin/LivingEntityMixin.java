package dev.craftv.client.mixin;

import dev.craftv.client.passthrough.HostCamera;
import dev.craftv.client.passthrough.PlayerSync;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.world.entity.LivingEntity;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/** The owner is placed every frame, so their own tick movement is ~0: animate the walk from the host's movement. */
@Mixin(LivingEntity.class)
abstract class LivingEntityMixin {
	private static final float WALK_SCALE = 4.0F;

	@Inject(method = "updateWalkAnimation", at = @At("HEAD"), cancellable = true)
	private void craftv$hostWalk(float distance, CallbackInfo ci) {
		if ((Object) this instanceof LocalPlayer player && HostCamera.live() != null) {
			player.walkAnimation.update(Math.min(PlayerSync.tickDistance() * WALK_SCALE, 1.0F), 0.4F, 1.0F);
			ci.cancel();
		}
	}
}
