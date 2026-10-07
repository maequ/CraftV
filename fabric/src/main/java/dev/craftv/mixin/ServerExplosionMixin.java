package dev.craftv.mixin;

import dev.craftv.coop.GtaWorld;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.ServerExplosion;
import net.minecraft.world.phys.Vec3;
import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/** Every explosion in a server world (TNT, creepers) also goes off in the GTA players' games (PROTOCOL.md §7.20). */
@Mixin(ServerExplosion.class)
abstract class ServerExplosionMixin {
	@Shadow @Final private ServerLevel level;
	@Shadow @Final private Vec3 center;
	@Shadow @Final private float radius;

	@Inject(method = "explode", at = @At("HEAD"))
	private void craftv$toGta(CallbackInfoReturnable<Integer> cir) {
		GtaWorld.onExplosion(this.level, this.center, this.radius);
	}
}
