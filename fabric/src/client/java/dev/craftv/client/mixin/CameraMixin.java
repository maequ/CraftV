package dev.craftv.client.mixin;

import dev.craftv.client.passthrough.FrameExporter;
import dev.craftv.client.passthrough.HostCamera;
import dev.craftv.client.passthrough.PlayerSync;
import dev.craftv.link.Messages;
import net.minecraft.client.Camera;
import net.minecraft.client.DeltaTracker;
import net.minecraft.client.Minecraft;
import net.minecraft.world.entity.Entity;
import org.joml.Quaternionf;
import org.joml.Vector3f;
import org.joml.Vector3fc;
import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * While the passthrough is on, the host's camera replaces the player's: position, rotation (with roll), field of
 * view and first/third person (PROTOCOL.md §7.15). Adapted from minecraft-gta5-passthrough (rehan-remade, MIT).
 */
@Mixin(Camera.class)
abstract class CameraMixin {
	private static final float DEG = (float) (Math.PI / 180.0);
	// The host pulled its camera into the head (against a wall): hide the model. Hysteresis so it doesn't flicker.
	private static final double INSIDE_HEAD = 0.6, OUTSIDE_HEAD = 1.0;
	private static boolean craftv$inside;
	@Shadow @Final private static Vector3fc FORWARDS;
	@Shadow @Final private static Vector3fc UP;
	@Shadow @Final private static Vector3fc LEFT;
	@Shadow @Final private Vector3f forwards;
	@Shadow @Final private Vector3f up;
	@Shadow @Final private Vector3f left;
	@Shadow @Final private Quaternionf rotation;
	@Shadow private float xRot;
	@Shadow private float yRot;
	@Shadow private boolean detached;
	@Shadow private int matrixPropertiesDirty;
	@Shadow private float depthFar;

	@Shadow
	protected abstract void setPosition(double x, double y, double z);

	@Inject(method = "update", at = @At("HEAD"))
	private void craftv$beginFrame(DeltaTracker deltaTracker, CallbackInfo ci) {
		HostCamera.beginFrame();
		PlayerSync.frame();
	}

	@Inject(method = "alignWithEntity", at = @At("TAIL"))
	private void craftv$hostCamera(float partialTicks, CallbackInfo ci) {
		Messages.Camera c = HostCamera.frame();
		if (c == null) {
			return;
		}
		this.xRot = c.pitch();
		this.yRot = c.yaw();
		this.rotation.rotationYXZ((float) Math.PI - c.yaw() * DEG, -c.pitch() * DEG, c.roll() * DEG);
		FORWARDS.rotate(this.rotation, this.forwards);
		UP.rotate(this.rotation, this.up);
		LEFT.rotate(this.rotation, this.left);
		this.matrixPropertiesDirty |= 3;
		this.setPosition(c.x(), c.y(), c.z());
		Entity player = Minecraft.getInstance().player;
		double d2 = player != null ? player.getEyePosition(partialTicks).distanceToSqr(c.x(), c.y(), c.z()) : 1e9;
		craftv$inside = craftv$inside ? d2 < OUTSIDE_HEAD * OUTSIDE_HEAD : d2 < INSIDE_HEAD * INSIDE_HEAD;
		this.detached = !c.firstPerson() && !craftv$inside;
	}

	@Inject(method = "calculateFov", at = @At("HEAD"), cancellable = true)
	private void craftv$hostFov(float partialTicks, CallbackInfoReturnable<Float> cir) {
		Messages.Camera c = HostCamera.frame();
		if (c != null) {
			cir.setReturnValue(c.fovY());
		}
	}

	@Inject(method = "update", at = @At("TAIL"))
	private void craftv$planes(DeltaTracker deltaTracker, CallbackInfo ci) {
		FrameExporter.setFar(this.depthFar);
	}
}
