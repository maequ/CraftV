package dev.craftv.client.mixin;

import com.llamalad7.mixinextras.injector.ModifyExpressionValue;
import dev.craftv.client.passthrough.HostCamera;
import net.minecraft.client.Minecraft;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;

/**
 * Holding attack only keeps breaking a block while the mouse is grabbed, and the hidden Minecraft never has the
 * mouse (the host window does). While the passthrough is on, a forwarded held attack mines like a held mouse.
 */
@Mixin(Minecraft.class)
abstract class MinecraftMixin {
	@ModifyExpressionValue(method = "handleKeybinds", at = @At(value = "INVOKE", target = "Lnet/minecraft/client/MouseHandler;isMouseGrabbed()Z"))
	private boolean craftv$hostHoldsTheMouse(boolean grabbed) {
		return grabbed || HostCamera.live() != null;
	}
}
