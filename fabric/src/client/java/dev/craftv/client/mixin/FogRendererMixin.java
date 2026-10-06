package dev.craftv.client.mixin;

import dev.craftv.client.passthrough.HostCamera;
import net.minecraft.client.renderer.fog.FogData;
import net.minecraft.client.renderer.fog.FogRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * No distance fog while the passthrough is on (the host has its own), and a black fog colour: the level pass clears
 * to (fog colour, alpha 0), so empty pixels come out (0, 0, 0, 0) and the world layer premultiplied. Adapted from
 * minecraft-gta5-passthrough (rehan-remade, MIT).
 */
@Mixin(FogRenderer.class)
abstract class FogRendererMixin {
	private static final float FAR_AWAY = 1.0E7F;

	@Inject(method = "updateBuffer", at = @At("HEAD"))
	private void craftv$noFog(FogData fog, CallbackInfo ci) {
		if (HostCamera.frame() != null) {
			fog.environmentalStart = FAR_AWAY;
			fog.environmentalEnd = FAR_AWAY * 2.0F;
			fog.renderDistanceStart = FAR_AWAY;
			fog.renderDistanceEnd = FAR_AWAY * 2.0F;
			fog.skyEnd = FAR_AWAY * 2.0F;
			fog.cloudEnd = FAR_AWAY * 2.0F;
			fog.color.set(0.0F, 0.0F, 0.0F, 0.0F);
		}
	}
}
