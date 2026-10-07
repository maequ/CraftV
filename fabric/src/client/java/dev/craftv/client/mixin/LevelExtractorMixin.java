package dev.craftv.client.mixin;

import static dev.craftv.link.Proto.*;

import dev.craftv.client.passthrough.HostCamera;
import dev.craftv.client.passthrough.ViewOptions;
import dev.craftv.link.Messages;
import dev.craftv.terrain.TerrainIndex;
import net.minecraft.client.Camera;
import net.minecraft.client.Minecraft;
import net.minecraft.client.player.LocalPlayer;
import net.minecraft.client.renderer.culling.Frustum;
import net.minecraft.client.renderer.extract.LevelExtractor;
import net.minecraft.client.renderer.state.level.LevelRenderState;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.BlockHitResult;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * The owner's body and the block outline in the passthrough.
 * <ul>
 * <li>Vanilla only draws an entity once the chunk section it stands in has been meshed (javap on 26.3:
 * isEntityVisible checks isSectionCompiledAndVisible). CraftV re-meshes sections whenever ground arrives, so after a
 * drive the owner stayed invisible for seconds (Sary: "my character keeps going invisible and coming back"). The
 * owner is always drawn while the host composites (seated in a car unless the host's settings say otherwise).</li>
 * <li>The outline box isn't drawn on the hidden ground (it outlined the street everywhere), or at all if turned off.</li>
 * </ul>
 */
@Mixin(LevelExtractor.class)
abstract class LevelExtractorMixin {
	@Inject(method = "isEntityVisible", at = @At("HEAD"), cancellable = true)
	private void craftv$ownerAlwaysDrawn(Entity entity, Frustum frustum, double camX, double camY, double camZ, float partialTicks, long now,
		CallbackInfoReturnable<Boolean> cir) {
		Messages.Camera c = HostCamera.frame();
		if (c != null && entity instanceof LocalPlayer) {
			cir.setReturnValue((c.flags() & CAMERA_IN_VEHICLE) == 0 || ViewOptions.vehicleBody);
		}
	}

	@Inject(method = "extractBlockOutline", at = @At("HEAD"), cancellable = true)
	private void craftv$outline(Camera camera, LevelRenderState state, CallbackInfo ci) {
		if (HostCamera.frame() == null) {
			return;
		}
		int mode = ViewOptions.outline;
		if (mode == 0) {
			ci.cancel();
			return;
		}
		Minecraft minecraft = Minecraft.getInstance();
		if (mode == 1 && minecraft.level != null && minecraft.hitResult instanceof BlockHitResult hit) {
			BlockPos pos = hit.getBlockPos();
			if (TerrainIndex.isTerrain(pos.getX(), pos.getY(), pos.getZ(), minecraft.level.getBlockState(pos))) {
				ci.cancel();
			}
		}
	}
}
