#!/usr/bin/env python3
"""Fetch an x64-hosted, x64-targeting MSVC toolset and the Windows SDK into DEST.

  python3 tools/msvc-wine/fetch_msvc.py DEST [--channel URL] [--vc VERSION] [--sdk Win11SDK_10.0.26100]

Reads the Visual Studio channel manifest (default VS 2022 release), downloads the
MSVC compiler/CRT/STL vsix packages (en-US resources) and the Windows SDK
header/library MSIs with their cabs, checks every payload's sha256 and unpacks:
  DEST/VC/Tools/MSVC/<ver>/{bin/Hostx64/x64,include,lib/x64}
  DEST/Windows Kits/10/{Include,Lib}/<sdk ver>/...
Afterwards run tools/msvc-wine/case_links.py DEST (clang-cl on a case-sensitive file system).
clang-cl from Ubuntu's llvm-19 packages needs a clang-cl link to clang
(ln -s /usr/lib/llvm-19/bin/clang /usr/local/bin/clang-cl); the real-MSVC
toolchain (msvc-wine-x86_64.cmake) needs wine only.
Needs msiextract (msitools). By using it you accept the Visual Studio and Windows
SDK license terms (https://go.microsoft.com/fwlink/?LinkId=2086102).
"""
import argparse, hashlib, json, os, re, shutil, subprocess, sys, urllib.request, zipfile

ap = argparse.ArgumentParser()
ap.add_argument("dest")
ap.add_argument("--channel", default="https://aka.ms/vs/17/release/channel")
ap.add_argument("--vc", help="e.g. 14.44.17.14 (default: newest in the manifest)")
ap.add_argument("--sdk", default="Win11SDK_10.0.26100")
a = ap.parse_args()
dest = os.path.abspath(a.dest)
cache = os.path.join(dest, ".cache")
os.makedirs(cache, exist_ok=True)


def fetch(url, name, sha=None):
    path = os.path.join(cache, name)
    if not os.path.exists(path):
        tmp = path + ".part"
        urllib.request.urlretrieve(url, tmp)
        os.replace(tmp, path)
    if sha:
        h = hashlib.sha256(open(path, "rb").read()).hexdigest()
        if h.lower() != sha.lower():
            sys.exit(f"sha256 mismatch: {name}")
    return path


channel = json.load(open(fetch(a.channel, "channel.json")))
item = next(i for i in channel["channelItems"] if i["id"] == "Microsoft.VisualStudio.Manifests.VisualStudio")
vsman = json.load(open(fetch(item["payloads"][0]["url"], "vs.json")))
packages = vsman["packages"]

vers = sorted({re.match(r"Microsoft\.VC\.([\d.]+)\.Tools\.HostX64\.TargetX64\.base$", p["id"]).group(1)
               for p in packages if re.match(r"Microsoft\.VC\.[\d.]+\.Tools\.HostX64\.TargetX64\.base$", p["id"])},
              key=lambda v: [int(x) for x in v.split(".")])
vc = a.vc or vers[-1]
wanted = re.compile(rf"Microsoft\.VC\.{re.escape(vc)}\.(Tools\.HostX64\.TargetX64(\.Res)?|CRT\.Headers|"
                    rf"CRT\.Headers\.Resources|CRT\.x64\.(Desktop|Store)|Premium\.Tools\.HostX64\.TargetX64|"
                    rf"CRT\.Redist\.X64)\.base$")
picked = [p for p in packages if wanted.match(p["id"]) and p.get("language") in (None, "neutral", "en-US")]
print(f"MSVC {vc}: {len(picked)} packages")
for p in picked:
    for pl in p["payloads"]:
        zpath = fetch(pl["url"], pl["fileName"], pl.get("sha256"))
        with zipfile.ZipFile(zpath) as z:
            for n in z.namelist():
                if n.startswith("Contents/") and not n.endswith("/"):
                    out = os.path.join(dest, n[len("Contents/"):].replace("%20", " "))
                    os.makedirs(os.path.dirname(out), exist_ok=True)
                    with z.open(n) as src, open(out, "wb") as dst:
                        shutil.copyfileobj(src, dst)

sdk = next(p for p in packages if p["id"] == a.sdk)
by_name = {pl["fileName"].split("\\")[-1]: pl for pl in sdk["payloads"]}
msis = [n for n in by_name if re.match(
    r"(Windows SDK Desktop (Headers x64|Headers x86|Libs x64)|Universal CRT Headers Libraries and Sources|"
    r"Windows SDK for Windows Store Apps (Headers|Libs|Contracts|Headers OnecoreUap))-x86_en-us\.msi$", n)]
print(f"{a.sdk}: {len(msis)} MSIs")
sdkdir = os.path.join(cache, "sdk")
os.makedirs(sdkdir, exist_ok=True)
for n in msis:
    pl = by_name[n]
    path = fetch(pl["url"], "sdk/" + n, pl.get("sha256"))
    for cab in sorted(set(re.findall(rb"[0-9a-f]{32}\.cab", open(path, "rb").read()))):
        cab = cab.decode()
        if cab in by_name:
            fetch(by_name[cab]["url"], "sdk/" + cab, by_name[cab].get("sha256"))
    subprocess.run(["msiextract", "-C", dest, path], check=True, stdout=subprocess.DEVNULL)
# msiextract keeps the MSI's "Program Files" prefix; the toolchain files expect
# DEST/Windows Kits (the Visual Studio layout).
kits = os.path.join(dest, "Program Files", "Windows Kits")
if os.path.isdir(kits) and not os.path.exists(os.path.join(dest, "Windows Kits")):
    shutil.move(kits, os.path.join(dest, "Windows Kits"))
print("done:", dest)
print("next: python3 tools/msvc-wine/case_links.py", dest, "(lowercase aliases for clang-cl);")
print("clang-cl: Ubuntu's clang-19 package has no clang-cl-19; link one to clang-19:")
print("  ln -s /usr/lib/llvm-19/bin/clang /usr/local/bin/clang-cl")
