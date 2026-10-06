package dev.craftv.mixin;

import dev.craftv.BlockSync;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.LevelChunk;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Every block change in a loaded chunk passes through LevelChunk.setBlockState (verified by javap
 * on 26.3: {@code BlockState setBlockState(BlockPos, BlockState, int)}, returns the old state or
 * null when nothing changed). Server-side changes become authoritative BLOCK_SETs for the host.
 */
@Mixin(LevelChunk.class)
public abstract class LevelChunkMixin {
	@Inject(method = "setBlockState", at = @At("RETURN"))
	private void craftv$reportChange(BlockPos pos, BlockState state, int flags, CallbackInfoReturnable<BlockState> cir) {
		if (cir.getReturnValue() == null) {
			return;
		}
		LevelChunk self = (LevelChunk) (Object) this;
		if (self.getLevel().isClientSide() && !BlockSync.clientIsAuthority()) {
			return;
		}
		BlockSync.onBlockChanged(self.getLevel(), pos.immutable(), state);
	}
}
