package dev.craftv.client.mixin;

import dev.craftv.client.passthrough.HostCamera;
import dev.craftv.client.passthrough.ViewOptions;
import net.minecraft.client.CameraType;
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.client.gui.Hud;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.Redirect;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/** The host's settings for Minecraft's HUD in the passthrough: the crosshair (in third person too) and the hotbar. */
@Mixin(Hud.class)
abstract class HudMixin {
	@Inject(method = "extractCrosshair", at = @At("HEAD"), cancellable = true)
	private void craftv$crosshairOff(GuiGraphicsExtractor graphics, DeltaTracker delta, CallbackInfo ci) {
		if (HostCamera.frame() != null && !ViewOptions.crosshair) {
			ci.cancel();
		}
	}

	@Redirect(method = "extractCrosshair", at = @At(value = "INVOKE", target = "Lnet/minecraft/client/CameraType;isFirstPerson()Z"))
	private boolean craftv$crosshairInThirdPerson(CameraType type) {
		return type.isFirstPerson() || (HostCamera.frame() != null && ViewOptions.crosshair);
	}

	@Inject(method = "extractHotbarAndDecorations", at = @At("HEAD"), cancellable = true)
	private void craftv$hudOff(GuiGraphicsExtractor graphics, DeltaTracker delta, CallbackInfo ci) {
		if (HostCamera.frame() != null && !ViewOptions.hud) {
			ci.cancel();
		}
	}
}
