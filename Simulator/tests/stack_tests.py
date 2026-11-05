#!/usr/bin/env python3
"""Runs every tests/stack/*.s on both pipelines and with batched/naive macro expansion.
Header lines in each file:   ; EXPECT: <FAULT_NAME | OK | ASM_ERROR>     ; REGS: r1=11 r2=22
Usage: python stack_tests.py [path/to/risc201sim]"""
import glob, os, re, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
sim = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "risc201sim")
extra = [  # (file, flags, expected) -- flag variants from the reference harness
    ("03_linear_overflow.s", ["--no-canary"], "SHADOW_STACK_MISMATCH"),
    ("03_linear_overflow.s", ["--no-canary", "--no-shadow"], "BAD_INSTRUCTION"),
]
def run(path, flags, pipeline):
    p = subprocess.run([sim, "--batch", "--pipeline", str(pipeline)] + flags + [path],
                       capture_output=True, text=True)
    return p.stdout + p.stderr
def verdict(out, exp, regs):
    stop = re.search(r'stop="([^"]*)"', out)
    stop = stop.group(1) if stop else out.strip()
    if exp == "ASM_ERROR":
        return ("error" in out and "duplicate" in out.lower()), stop
    if exp == "OK":
        ok = stop.startswith("HALT")
    else:
        ok = stop.startswith("EXCEPTION " + exp)
    if ok and regs:
        for r in regs.split():
            k, v = r.split("="); m = re.search(r"\b%s=(\d+)" % k, out)
            if not m or int(m.group(1)) != int(v): ok = False
    return ok, stop
fails = total = 0
cases = []
for f in sorted(glob.glob(os.path.join(here, "stack", "*.s"))):
    txt = open(f).read()
    exp = re.search(r"EXPECT:\s*(\S+)", txt).group(1)
    regs = (re.search(r"REGS:\s*(.*)", txt) or [None, ""])[1]
    cases.append((f, [], exp, regs))
for name, flags, exp in extra:
    cases.append((os.path.join(here, "stack", name), flags, exp, ""))
for f, flags, exp, regs in cases:
    results = []
    for pipeline in (6, 4):
        for naive in ([False, True] if not flags else [False]):
            fl = flags + (["--naive"] if naive else [])
            out = run(f, fl, pipeline)
            ok, stop = verdict(out, exp, regs)
            results.append((pipeline, naive, ok, stop, out))
    good = all(r[2] for r in results)
    total += 1; fails += (not good)
    print("[%s] %-26s %s %s" % ("PASS" if good else "FAIL", os.path.basename(f), " ".join(flags), exp))
    if not good:
        for pl, nv, ok, stop, out in results:
            if not ok: print("      pipeline=%d naive=%s -> %s" % (pl, nv, stop))
# batching on/off must give identical architectural results for programs that finish
for f in sorted(glob.glob(os.path.join(here, "stack", "0[12]_*.s")) + glob.glob(os.path.join(here, "stack", "17_*.s"))):
    # r15 holds a code address (return address), which legitimately differs once batching shortens the code
    regs = lambda o: [x for x in re.findall(r"\br\d+=\d+", o) if not x.startswith("r15=")]
    a = regs(run(f, [], 6)); b = regs(run(f, ["--naive"], 6))
    total += 1
    ok = a == b
    fails += (not ok)
    print("[%s] batched == naive registers: %s" % ("PASS" if ok else "FAIL", os.path.basename(f)))
print("\n%d/%d checks passed" % (total - fails, total))
sys.exit(1 if fails else 0)
