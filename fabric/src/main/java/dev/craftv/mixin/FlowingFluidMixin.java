package dev.craftv.mixin;

import dev.craftv.terrain.TerrainIndex;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FlowingFluid;
import net.minecraft.world.level.material.FluidState;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Terrain water is the host game's sea and lakes, cut off at chunk and shore edges: it must stay put. In Sary's first
 * passthrough run, placing a block next to it made it run down the street (dozens of water BLOCK_SETs). Water anyone
 * places themselves still flows normally.
 */
@Mixin(FlowingFluid.class)
abstract class FlowingFluidMixin {
	@Inject(method = "spread", at = @At("HEAD"), cancellable = true)
	private void craftv$terrainWaterStays(ServerLevel level, BlockPos pos, BlockState state, FluidState fluid, CallbackInfo ci) {
		if (fluid.isSource() && TerrainIndex.isTerrainWater(pos.getX(), pos.getY(), pos.getZ())) {
			ci.cancel();
		}
	}
}
