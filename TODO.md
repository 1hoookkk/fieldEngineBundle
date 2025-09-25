# EMU Plugin TODO List

## 🔴 Critical (Must Have)
- [x] Implement filter models (HyperQ 12/6, PhaserForm, VocalMorph)
- [ ] Load filter coefficients from ref/emu_banks/
- [x] Basic modulation matrix (LFO → Cutoff, Env → Cutoff)
- [ ] Preset loading system
- [x] Connect EMU parameters to existing FieldEngine params

## 🟡 Important (Should Have)
- [ ] Band splitter (4-8 bands)
- [ ] Per-band filter routing
- [ ] Morphing between filter models
- [ ] Key tracking
- [ ] Velocity sensitivity

## 🟢 Nice to Have
- [ ] AI preset assistant
- [ ] Parameter animation system
- [ ] Advanced modulation routing
- [ ] Filter model blending
- [ ] Preset morphing

## 📝 Quick Wins
- [x] Add filter model dropdown to UI (in parameters)
- [x] Wire up T1/T2 knobs to cutoff/resonance (using Z-plane tables)
- [ ] Basic preset browser
- [ ] Simple envelope → filter connection

## 🧪 Testing
- [ ] Compare filter response to EMU reference
- [ ] Validate preset loading
- [ ] Check CPU performance
- [ ] Test modulation accuracy
