package dev.craftv.client.mixin;

import static dev.craftv.link.Proto.*;

import dev.craftv.client.passthrough.HostCamera;
import dev.craftv.link.Messages;
import net.minecraft.client.model.HumanoidModel;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.client.renderer.entity.player.AvatarRenderer;
import net.minecraft.client.renderer.entity.state.AvatarRenderState;
import net.minecraft.world.entity.Avatar;
import net.minecraft.world.entity.Pose;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * The owner's body does what the host's player does: sits in a car (PROTOCOL.md §7.15 IN_VEHICLE), holds the phone
 * up to their face while the host's player uses theirs (PHONE), and always stands (never the swimming pose: GTA
 * does the swimming).
 */
@Mixin(AvatarRenderer.class)
abstract class AvatarRendererMixin {
	private static final float SEATED_SCALE = 0.78F;
	private static final int GRENADE = 7; // Guns.IDS: thrown, not aimed

	@Inject(method = "extractRenderState(Lnet/minecraft/world/entity/Avatar;Lnet/minecraft/client/renderer/entity/state/AvatarRenderState;F)V",
		at = @At("TAIL"))
	private void craftv$hostPose(Avatar entity, AvatarRenderState state, float partialTicks, CallbackInfo ci) {
		Messages.Camera c = HostCamera.frame();
		if (c == null || !(entity instanceof LocalPlayer)) {
			return;
		}
		boolean crouching = entity.isCrouching() && (c.flags() & CAMERA_IN_VEHICLE) == 0;
		state.pose = crouching ? Pose.CROUCHING : Pose.STANDING; // Ctrl in the host game is Minecraft's sneak (v1.5)
		state.isCrouching = crouching;
		if ((c.flags() & CAMERA_IN_VEHICLE) != 0) {
			state.isPassenger = true;
			// Steve is taller than a car's seat allows: his head came out of the roof (Sary, 2026-10-07). Smaller while seated.
			state.scale *= SEATED_SCALE;
		}
		int gun = dev.craftv.coop.Guns.index(entity.getMainHandItem());
		if (gun >= 0 && gun != GRENADE && !crouching) {
			state.rightArmPose = HumanoidModel.ArmPose.CROSSBOW_HOLD; // both arms up, aiming: GTA's gun is in his hands
		}
		if ((c.flags() & CAMERA_PHONE) != 0) {
			state.rightArmPose = HumanoidModel.ArmPose.SPYGLASS; // the phone held up to his face
			state.leftArmPose = HumanoidModel.ArmPose.EMPTY;
		}
	}
}
