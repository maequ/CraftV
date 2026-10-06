package dev.craftv.client.mixin;

import net.minecraft.client.KeyMapping;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** Forwarded presses count as clicks (ClientInput). */
@Mixin(KeyMapping.class)
public interface KeyMappingAccessor {
	@Accessor("clickCount")
	int craftv$getClickCount();

	@Accessor("clickCount")
	void craftv$setClickCount(int clickCount);
}
