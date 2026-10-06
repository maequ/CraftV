package dev.craftv.client.mixin;

import dev.craftv.client.passthrough.Passthrough;
import dev.craftv.terrain.TerrainIndex;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.material.FluidState;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Terrain water the owner's view hides (GTA draws its own sea) still ticked its ambient effects: water spawns
 * UNDERWATER particles, the black specks Sary saw all over the picture. Not around hidden terrain water.
 */
@Mixin(FluidState.class)
abstract class HiddenWaterMixin {
	@Inject(method = "animateTick", at = @At("HEAD"), cancellable = true)
	private void craftv$noHiddenWaterParticles(Level level, BlockPos pos, RandomSource random, CallbackInfo ci) {
		if (Passthrough.hideTerrain() && TerrainIndex.isTerrainWater(pos.getX(), pos.getY(), pos.getZ())) {
			ci.cancel();
		}
	}
}
