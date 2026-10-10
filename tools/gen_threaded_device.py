#!/usr/bin/env python3
"""Writes ThreadedRenderDevice (.h/.cpp) from IRenderDevice.h: a device that hands the
calls made on it to one other thread, in order, and is itself an IRenderDevice.

    python3 tools/gen_threaded_device.py            (run again when IRenderDevice changes)

Every virtual of IRenderDevice is one of:
  QUEUED   a void call of the frame's own traffic: put in the queue (what it points at is
           copied), run later by the device thread
  PURE     answers from its arguments alone, or from what is set up once: asked directly
  SHADOW   answered from what this side has already said (the target bound, a frame open)
  DIRECT   everything else - the queue is run dry first, then the real device is asked on
           the calling thread (the device thread is idle then: never two at once)
"""
import os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "include/Pyros3D/Rendering/Device/IRenderDevice.h")

QUEUED = set("""EndCommandBuffer Clear SetDepthTest SetDepthMask PrepareDepthClear SetStencilTestEnabled
SetClearStencilValue SetStencilFunction SetStencilOperation SetScissorRect SetScissorTestEnabled SetWireFrame SetColorMask
SetPolygonOffsetEnabled SetPolygonOffset SetBlendingEnabled SetBlendFunction SetBlendEquation SetCullFaceMode DisableCullFace
BindPipeline DestroyPipeline EnableClipDistance DisableClipDistance SetViewport UseProgram DeleteVertexArray BindVertexArray
BindArrayBuffer BindElementBuffer SetVertexAttribute SetFloatVertexAttribute DisableVertexAttribute SetVertexAttributeDivisor
BindUniformBlockIfPresent SetPointShadowCubeFacePass DrawArrays DrawElements DrawElementsInstanced UpdateUniformBuffer
ReplaceUniformBuffer DestroyUniformBuffer ReallocateBuffer UpdateBufferSubData DestroyBuffer DestroyComputePipeline
BindComputePipeline UpdateStorageBuffer BindStorageBuffer DestroyStorageBuffer Dispatch ComputeBarrier SendUniformInt
SendUniformFloat SendUniformVec2 SendUniformVec3 SendUniformVec4 SendUniformMatrix BindTextureToTarget
SetTextureWrapS SetTextureWrapT SetTextureWrapR SetTextureMagFilter SetTextureMinFilter SetTextureBaseMaxLevel
SetTextureBorderColor SetTextureCompareMode SetPixelUnpackAlignment ActivateTextureUnit GenerateMipmap DestroyFramebuffer
SetFramebufferPreserveDepth AttachFramebufferTexture2D AttachFramebufferRenderbuffer SetDrawBufferNone SetReadBufferNone
SetDrawBufferBack SetReadBufferBack SetDrawBuffers DestroyRenderbuffer BindRenderbuffer RenderbufferStorage
RenderbufferStorageMultisample SetMultisampleEnabled BlitFramebuffer CopyDepthTexture FlushOffscreenWork DeleteShaderStage
DeleteProgram DetachShaderStage AttachShaderStage""".split())
PURE = set("""TranslateBufferBit TranslateDrawType TranslateAttributeType TranslateTextureFormat TranslateTextureTarget
TranslateFramebufferAccess TranslateFramebufferAttachment TranslateFramebufferStatus TranslateRenderbufferFormat
TranslateShadowBiasMatrix TranslateProjectionMatrix IsVulkan NeedsManualDisplayGamma RenderTargetOriginIsTopLeft
SupportsCompute GetMaxComputeWorkGroupInvocations GetMaxComputeWorkGroupCount GetMaxSamples CanBlitResolveDepth
TemporalUpscalerId GetTextureDataSize GetTextureUploadSize GetAutoUniformBlockLayout IsProgram IsShaderStage BuildShaderSource GetSwapchainGeneration GetUniformLocation GetAttributeLocation""".split())
# written by hand in the .cpp's fixed part
SPECIAL = set("""BeginFrame EndFrame BindFramebuffer SetClearColor IsFrameInProgress GetCurrentRenderTarget BeginCommandBuffer WaitIdle
CreateBuffer CreateVertexArray CreatePipeline DestroyBuffer DeleteVertexArray DestroyPipeline
BeginParallelStream EnterParallelStream LeaveParallelStream NewDetachedStream PlaceStream
CreateUniformBuffer DestroyUniformBuffer
CreateTextureObject DestroyTextureObject UploadTexture2D RunTemporalUpscale""".split())
# Buffers, vertex arrays and pipelines are made in the queue like everything else (a
# game's HUD makes new buffers every frame: waiting for each was most of a frame), so the
# handle given out here is not the device's own: these arguments are turned into it,
# on the device thread, when the call is made.
HANDLES = {"BindArrayBuffer": ["buffer"], "BindElementBuffer": ["buffer"], "UpdateBufferSubData": ["buffer"],
           "ReallocateBuffer": ["buffer"], "MapBuffer": ["buffer"], "UnmapBuffer": ["buffer"],
           "BindVertexArray": ["vao"], "BindPipeline": ["pipeline"],
           # (uniform buffers too: a material makes its own the first time it is drawn)
           "UpdateUniformBuffer": ["buffer"], "ReplaceUniformBuffer": ["buffer"], "BindUniformBlockIfPresent": ["bufferHandle"],
           # (and textures: one that arrives in the middle of a game is made, bound and filled without a wait)
           "BindTextureToTarget": ["texture"], "AttachFramebufferTexture2D": ["textureId"],
           "CopyDepthTexture": ["srcTexture", "dstTexture"], "GetImGuiTextureID": ["texture"]}
# what a queued call points at, and how many bytes of it there are
COPIES = {"UpdateUniformBuffer": ("data", "sizeBytes"), "ReplaceUniformBuffer": ("data", "sizeBytes"),
          "ReallocateBuffer": ("data", "length"), "UpdateBufferSubData": ("data", "length"),
          "UpdateStorageBuffer": ("data", "sizeBytes"),
          "SendUniformInt": ("data", "count * 4"), "SendUniformFloat": ("data", "count * 4"), "SendUniformVec2": ("data", "count * 8"),
          "SendUniformVec3": ("data", "count * 12"), "SendUniformVec4": ("data", "count * 16"), "SendUniformMatrix": ("data", "count * 64")}

def split_args(a):
    out, depth, cur = [], 0, ""
    for ch in a:
        if ch in "<(": depth += 1
        if ch in ">)": depth -= 1
        if ch == "," and depth == 0: out.append(cur.strip()); cur = ""
        else: cur += ch
    if cur.strip(): out.append(cur.strip())
    return out

def parse():
    methods = []
    for line in open(SRC):
        s = line.strip()
        if not s.startswith("virtual ") or "~" in s: continue
        m = re.match(r"virtual\s+(.*?)(\w+)\((.*)\)\s*(const)?\s*(=\s*0\s*;|\{.*|;)\s*$", s)
        if not m: raise SystemExit("cannot read: " + s)
        ret, name, args, const = m.group(1).strip(), m.group(2), m.group(3), m.group(4) or ""
        plist = []
        for a in split_args(args):
            a = re.sub(r"\s*=\s*[^,]+$", "", a)           # a default value
            pm = re.match(r"(.*?)(\w+)$", a)
            plist.append((pm.group(1).strip(), pm.group(2)))
        methods.append((ret, name, plist, const))
    return methods

def decayed(t):
    t = t.strip()
    if t.endswith("&"): t = re.sub(r"^const\s+", "", t[:-1].strip())
    return t

def main():
    M = parse()
    decl, body = [], []
    for ret, name, plist, const in M:
        sig = ", ".join("%s %s" % (t, n) for t, n in plist)
        call = ", ".join(n for _, n in plist)
        decl.append("\t\tvirtual %s %s(%s)%s override;" % (ret, name, sig, (" " + const) if const else ""))
        if name in SPECIAL: continue
        head = "\t%s ThreadedRenderDevice::%s(%s)%s\n\t{\n" % (ret, name, sig, (" " + const) if const else "")
        if name in PURE:
            body.append(head + "\t\t%sreal->%s(%s);\n\t}\n" % ("" if ret == "void" else "return ", name, call))
        elif name in QUEUED:
            assert ret == "void", name
            caps, pre, args = ["dev_ = real"], "", []
            copy = COPIES.get(name)
            for t, n in plist:
                if copy and n == copy[0]:
                    pre += "\t\tconst void* %s_ = %s ? Keep(%s, (size_t)(%s)) : NULL;\n" % (n, n, n, copy[1])
                    caps.append("%s_" % n)
                    args.append("(%s)%s_" % (t, n) if "f32" in t or "int32" in t else "%s_" % n)
                elif n in HANDLES.get(name, []):
                    caps.append(n)
                    if "self = this" not in caps: caps.append("self = this")
                    args.append("self->RealOf(%s)" % n)
                else:
                    caps.append("%s = %s(%s)" % (n, decayed(t), n) if t.strip().endswith("&") else n)
                    args.append(n)
            body.append(head + pre + "\t\tPush([%s]() { dev_->%s(%s); });\n\t}\n" % (", ".join(caps), name, ", ".join(args)))
        elif name in HANDLES:
            call2 = ", ".join(("RealOf(%s)" % n) if n in HANDLES[name] else n for _, n in plist)
            body.append(head + "\t\tDrain(\"%s\");\n\t\t%sreal->%s(%s);\n\t}\n" % (name, "" if ret == "void" else "return ", name, call2))
        else:
            body.append(head + "\t\t%sDrain(\"%s\");\n\t\t%sreal->%s(%s);\n\t}\n" % ("const_cast<ThreadedRenderDevice*>(this)->" if const else "", name, "" if ret == "void" else "return ", name, call))
    outdir_h = os.path.join(ROOT, "include/Pyros3D/Rendering/Device")
    outdir_c = os.path.join(ROOT, "src/Pyros3D/Rendering/Device")
    open(os.path.join(outdir_h, "ThreadedRenderDevice.generated.h"), "w").write(
        "// Written by tools/gen_threaded_device.py from IRenderDevice.h - not by hand.\n" + "\n".join(decl) + "\n")
    open(os.path.join(outdir_c, "ThreadedRenderDevice.generated.inl"), "w").write(
        "// Written by tools/gen_threaded_device.py from IRenderDevice.h - not by hand.\n" + "\n".join(body))
    print("%d methods: %d queued, %d asked directly, %d by hand, %d run the queue dry first" % (
        len(M), sum(1 for m in M if m[1] in QUEUED), sum(1 for m in M if m[1] in PURE), sum(1 for m in M if m[1] in SPECIAL),
        sum(1 for m in M if m[1] not in QUEUED | PURE | SPECIAL)))
    missing = (QUEUED | PURE | SPECIAL) - set(m[1] for m in M)
    if missing: print("named here and not in the interface:", sorted(missing))

if __name__ == "__main__": main()
