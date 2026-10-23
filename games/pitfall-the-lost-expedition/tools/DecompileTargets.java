// Prints, for each address argument: the containing function, its callers, and its decompiled C.
// Usage (headless): -postScript DecompileTargets.java 0x00173910 0x00180C10
// @category coop-anything

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

public class DecompileTargets extends GhidraScript {
    @Override
    protected void run() throws Exception {
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        for (String arg : getScriptArgs()) {
            if (arg.startsWith("refs:")) {
                Address data = toAddr(Long.decode(arg.substring(5)));
                println("==== references to " + data);
                for (Reference ref : getReferencesTo(data)) {
                    Function fn = getFunctionContaining(ref.getFromAddress());
                    println("  " + ref.getReferenceType() + " from " + ref.getFromAddress() + " in "
                            + (fn == null ? "?" : fn.getName() + " @ " + fn.getEntryPoint()));
                }
                continue;
            }
            Address addr = toAddr(Long.decode(arg));
            Function fn = getFunctionContaining(addr);
            if (fn == null) {
                disassemble(addr);
                fn = createFunction(addr, null);
            }
            println("==== " + arg + " -> " + (fn == null ? "no function" : fn.getName() + " @ " + fn.getEntryPoint()));
            if (fn == null) {
                continue;
            }
            for (Reference ref : getReferencesTo(fn.getEntryPoint())) {
                Function caller = getFunctionContaining(ref.getFromAddress());
                println("  called from " + ref.getFromAddress() + " in "
                        + (caller == null ? "?" : caller.getName() + " @ " + caller.getEntryPoint()));
            }
            DecompileResults res = decomp.decompileFunction(fn, 60, monitor);
            println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "  decompile failed: " + res.getErrorMessage());
        }
        decomp.dispose();
    }
}
