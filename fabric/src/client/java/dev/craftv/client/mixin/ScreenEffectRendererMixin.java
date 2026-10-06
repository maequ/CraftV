package dev.craftv.client.mixin;

import dev.craftv.client.passthrough.HostCamera;
import net.minecraft.client.renderer.ScreenEffectRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * No full-screen effects (inside a block, under water, on fire) over the host's picture while the passthrough is
 * on: the owner often stands inside terrain blocks that are GTA's ground, and the host draws its own water and fire.
 */
@Mixin(ScreenEffectRenderer.class)
abstract class ScreenEffectRendererMixin {
	@Inject(method = "submit", at = @At("HEAD"), cancellable = true)
	private void craftv$noScreenEffects(CallbackInfo ci) {
		if (HostCamera.frame() != null) {
			ci.cancel();
		}
	}
}
