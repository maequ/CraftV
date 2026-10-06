package dev.craftv.client.mixin;

import net.minecraft.client.MouseHandler;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** The host's cursor over an open Minecraft screen (inventory): ClientInput puts the mouse there. */
@Mixin(MouseHandler.class)
public interface MouseHandlerAccessor {
	@Accessor("xpos")
	void craftv$setXpos(double x);

	@Accessor("ypos")
	void craftv$setYpos(double y);
}
