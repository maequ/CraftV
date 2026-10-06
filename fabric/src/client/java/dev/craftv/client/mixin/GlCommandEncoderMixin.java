package dev.craftv.client.mixin;

import com.mojang.renderpearl.api.buffers.GpuBuffer;
import com.mojang.renderpearl.api.textures.GpuTexture;
import com.mojang.renderpearl.backend.opengl.GlStateManager;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Reading back a depth texture sets the read framebuffer's read buffer to GL_NONE and never restores it, so every
 * later colour readback through the same framebuffer fails. Vanilla never reads depth back; the frame export does
 * every frame. Found and fixed by minecraft-gta5-passthrough (rehan-remade, MIT).
 */
@Mixin(targets = "com.mojang.renderpearl.backend.opengl.GlCommandEncoder")
abstract class GlCommandEncoderMixin {
	private static final int GL_COLOR_ATTACHMENT0 = 0x8CE0;

	@Inject(method = "copyTextureToBuffer(Lcom/mojang/renderpearl/api/textures/GpuTexture;Lcom/mojang/renderpearl/api/buffers/GpuBuffer;JLjava/lang/Runnable;IIIII)V",
		at = @At(value = "INVOKE", target = "Lcom/mojang/renderpearl/backend/opengl/GlStateManager;_glFramebufferTexture2D(IIIII)V"))
	private void craftv$restoreReadBuffer(GpuTexture source, GpuBuffer destination, long offset, Runnable callback, int mipLevel, int x, int y, int width,
		int height, CallbackInfo ci) {
		if (source.getFormat().hasDepthAspect()) {
			GlStateManager._glReadBuffer(GL_COLOR_ATTACHMENT0);
		}
	}
}
