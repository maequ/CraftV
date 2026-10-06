package dev.craftv.client.mixin;

import dev.craftv.client.passthrough.HostCamera;
import dev.craftv.link.Messages;
import dev.craftv.link.Proto;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.client.renderer.culling.Frustum;
import net.minecraft.client.renderer.entity.EntityRenderDispatcher;
import net.minecraft.world.entity.Entity;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/** In a host vehicle the host draws its own driver (PROTOCOL.md §7.15 IN_VEHICLE): the owner's model isn't drawn. */
@Mixin(EntityRenderDispatcher.class)
abstract class EntityRenderDispatcherMixin {
	@Inject(method = "shouldRender", at = @At("HEAD"), cancellable = true)
	private <E extends Entity> void craftv$noOwnerInVehicle(E entity, Frustum frustum, double x, double y, double z, float partialTicks,
		CallbackInfoReturnable<Boolean> cir) {
		Messages.Camera c = HostCamera.frame();
		if (c != null && (c.flags() & Proto.CAMERA_IN_VEHICLE) != 0 && entity instanceof LocalPlayer) {
			cir.setReturnValue(false);
		}
	}
}
