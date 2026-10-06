package dev.craftv.client.mixin;

import dev.craftv.client.passthrough.Passthrough;
import dev.craftv.terrain.TerrainBuilder;
import dev.craftv.terrain.TerrainColumns;
import dev.craftv.terrain.TerrainIndex;
import net.minecraft.client.renderer.chunk.RenderSectionRegion;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.material.Fluids;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Unique;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * The owner's passthrough view leaves the ground to the host (brief §8): chunk meshes are built from this copy of
 * the world, so terrain blocks read as air here, and only here. The server, physics and friends' clients keep the
 * real blocks. One meshing thread uses a region at a time, so the last chunk's columns are cached per region.
 */
@Mixin(RenderSectionRegion.class)
abstract class RenderSectionRegionMixin {
	@Unique private long craftv$cachedChunk = Long.MIN_VALUE;
	@Unique private TerrainIndex.Columns craftv$cachedColumns;

	@Unique
	private TerrainColumns.Kind craftv$terrainKind(BlockPos pos) {
		int cx = pos.getX() >> 4, cz = pos.getZ() >> 4;
		long key = ((long) cx & 0xFFFFFFFFL) | (((long) cz & 0xFFFFFFFFL) << 32);
		if (key != craftv$cachedChunk) {
			craftv$cachedChunk = key;
			craftv$cachedColumns = TerrainIndex.get(cx, cz);
		}
		TerrainIndex.Columns c = craftv$cachedColumns;
		return c == null ? null : c.kindAt(pos.getX() & 15, pos.getY(), pos.getZ() & 15);
	}

	@Inject(method = "getBlockState", at = @At("RETURN"), cancellable = true)
	private void craftv$hideTerrain(BlockPos pos, CallbackInfoReturnable<BlockState> cir) {
		BlockState state = cir.getReturnValue();
		if (!Passthrough.hideTerrain() || state.isAir()) {
			return;
		}
		TerrainColumns.Kind kind = craftv$terrainKind(pos);
		if (kind != null && TerrainBuilder.state(kind) == state) {
			cir.setReturnValue(Blocks.AIR.defaultBlockState());
		}
	}

	@Inject(method = "getFluidState", at = @At("RETURN"), cancellable = true)
	private void craftv$hideTerrainWater(BlockPos pos, CallbackInfoReturnable<FluidState> cir) {
		if (Passthrough.hideTerrain() && !cir.getReturnValue().isEmpty() && craftv$terrainKind(pos) == TerrainColumns.Kind.WATER) {
			cir.setReturnValue(Fluids.EMPTY.defaultFluidState());
		}
	}
}
