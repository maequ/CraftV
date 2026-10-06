package dev.craftv.client.mixin;

import dev.craftv.client.passthrough.Passthrough;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.tags.TagKey;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.material.Fluid;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * GTA moves the owner and GTA does the swimming: in Minecraft they're never "in water" while the passthrough is on.
 * Otherwise standing in (hidden) terrain water put them in the swimming pose, showed air bubbles and the swim
 * animation, and hid the body under the surface in third person.
 */
@Mixin(Entity.class)
abstract class OwnerDryMixin {
	private boolean craftv$dry() {
		return (Object) this instanceof LocalPlayer && Passthrough.hideTerrain();
	}

	@Inject(method = "isInWater", at = @At("HEAD"), cancellable = true)
	private void craftv$notInWater(CallbackInfoReturnable<Boolean> cir) {
		if (craftv$dry()) {
			cir.setReturnValue(false);
		}
	}

	@Inject(method = "isUnderWater", at = @At("HEAD"), cancellable = true)
	private void craftv$notUnderWater(CallbackInfoReturnable<Boolean> cir) {
		if (craftv$dry()) {
			cir.setReturnValue(false);
		}
	}

	@Inject(method = "isEyeInFluid", at = @At("HEAD"), cancellable = true)
	private void craftv$eyesDry(TagKey<Fluid> fluid, CallbackInfoReturnable<Boolean> cir) {
		if (craftv$dry()) {
			cir.setReturnValue(false);
		}
	}
}
