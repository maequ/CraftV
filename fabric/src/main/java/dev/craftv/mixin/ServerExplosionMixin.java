package dev.craftv.mixin;

import dev.craftv.coop.GtaWorld;
import dev.craftv.terrain.TerrainIndex;
import dev.craftv.terrain.TerrainService;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.ServerExplosion;
import net.minecraft.world.phys.Vec3;
import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Every explosion in a server world (TNT, creepers) also goes off in the GTA players' games (PROTOCOL.md §7.20). In
 * the GTA world it leaves the copied GTA ground alone: GTA's street can't break, so a crater only in Minecraft was an
 * invisible hole (Sary, 2026-10-07: "did I actually break the floor?"). Blocks people placed still blow up.
 */
@Mixin(ServerExplosion.class)
abstract class ServerExplosionMixin {
	@Shadow @Final private ServerLevel level;
	@Shadow @Final private Vec3 center;
	@Shadow @Final private float radius;

	@Inject(method = "explode", at = @At("HEAD"))
	private void craftv$toGta(CallbackInfoReturnable<Integer> cir) {
		GtaWorld.onExplosion(this.level, this.center, this.radius);
	}

	@Inject(method = "calculateExplodedPositions", at = @At("RETURN"), cancellable = true)
	private void craftv$groundStays(CallbackInfoReturnable<List<BlockPos>> cir) {
		if (this.level != TerrainService.mirror(this.level.getServer())) {
			return;
		}
		List<BlockPos> kept = new ArrayList<>(cir.getReturnValue().size());
		for (BlockPos pos : cir.getReturnValue()) {
			if (!TerrainIndex.isTerrain(pos.getX(), pos.getY(), pos.getZ(), this.level.getBlockState(pos))) {
				kept.add(pos);
			}
		}
		cir.setReturnValue(kept);
	}
}
