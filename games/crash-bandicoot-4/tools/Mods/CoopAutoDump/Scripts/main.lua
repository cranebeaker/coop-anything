-- CoopAutoDump: written for the Crash 4 co-op project.
-- 10 seconds after UE4SS starts, writes a list of every game object (UE4SS_ObjectDump.txt)
-- and C++ headers describing the game's classes (CXXHeaderDump folder). Nothing in the game is changed.
print("[CoopAutoDump] loaded; dumping in 10 seconds\n")
ExecuteWithDelay(10000, function()
    print("[CoopAutoDump] DumpAllObjects start\n")
    DumpAllObjects()
    print("[CoopAutoDump] DumpAllObjects done; GenerateSDK start\n")
    GenerateSDK()
    print("[CoopAutoDump] GenerateSDK done\n")
end)
