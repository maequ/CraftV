package dev.craftv.client.mixin;

import com.mojang.blaze3d.pipeline.RenderTarget;
import dev.craftv.client.passthrough.FrameExporter;
import dev.craftv.client.passthrough.HostCamera;
import net.minecraft.client.renderer.GameRenderer;
import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.ModifyArg;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * No sky while the passthrough is on (the host's sky shows through), and the frame split into the world and overlay
 * layers for the export (PROTOCOL.md §11). Adapted from minecraft-gta5-passthrough (rehan-remade, MIT).
 */
@Mixin(GameRenderer.class)
abstract class GameRendererMixin {
	@Shadow @Final private RenderTarget mainRenderTarget;

	@ModifyArg(method = "renderLevel", at = @At(value = "INVOKE",
		target = "Lnet/minecraft/client/renderer/LevelRenderer;render(Lcom/mojang/blaze3d/resource/GraphicsResourceAllocator;ZLnet/minecraft/client/renderer/state/level/CameraRenderState;Lcom/mojang/renderpearl/api/buffers/GpuBufferSlice;Lorg/joml/Vector4f;ZZ)V"),
		index = 5)
	private boolean craftv$noSky(boolean shouldRenderSky) {
		return shouldRenderSky && HostCamera.frame() == null;
	}

	@Inject(method = "renderLevel", at = @At(value = "INVOKE",
		target = "Lnet/minecraft/client/renderer/GameRenderer;render3dHud(Lnet/minecraft/client/renderer/state/level/CameraRenderState;Lnet/minecraft/client/renderer/state/level/PlayerRenderState;Lnet/minecraft/client/renderer/state/OptionsRenderState;Z)V"))
	private void craftv$captureWorld(CallbackInfo ci) {
		FrameExporter.captureWorld(this.mainRenderTarget);
	}

	@Inject(method = "renderItemInHand", at = @At("HEAD"), cancellable = true)
	private void craftv$handOff(net.minecraft.client.renderer.state.level.CameraRenderState camera, net.minecraft.client.renderer.state.level.PlayerRenderState player,
		com.mojang.renderpearl.api.textures.GpuTextureView target, CallbackInfo ci) {
		var c = HostCamera.frame();
		if (c != null && (!dev.craftv.client.passthrough.ViewOptions.hand || (c.flags() & dev.craftv.link.Proto.CAMERA_PHONE) != 0)) {
			ci.cancel(); // the host's settings hide the first-person hand, and the host's phone is in it now
		}
	}

	@Inject(method = "render", at = @At("TAIL"))
	private void craftv$captureOverlay(CallbackInfo ci) {
		FrameExporter.captureOverlay(this.mainRenderTarget);
	}
}
