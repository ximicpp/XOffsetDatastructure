# First Time Setup Guide

## 🎯 You're Here: Build directory not found

This means you need to build the project first. Follow these steps:

---

## Step 1: Check Your Environment

Run the diagnostic tool:

```cmd
cd wsl
wsl_diagnose.bat
```

This will tell you what's missing.

---

## Step 2: Install Prerequisites (First Time Only)

### If Clang P2996 is NOT installed:

```cmd
cd wsl

# Install basic tools (~5 minutes)
wsl_setup_tools.bat

# Build Clang P2996 compiler (~30-60 minutes)
wsl_build_clang_p2996.bat
```

**Note**: The compiler build takes time. This is a ONE-TIME setup.

---

## Step 3: Build the Project

```cmd
cd wsl
wsl_rebuild_with_reflection.bat
```

This will:
- Clean old builds
- Compile all examples
- Create `build_cpp26/bin/` directory

Expected output:
```
================================================
  Rebuild Project (C++26 Reflection Enabled)
================================================

Cleaning old build...
Starting build...
-- Configuring done
-- Generating done
-- Build files written to: build_cpp26
[100%] Built target xoffsetdatastructure2_demo

================================================
  Complete!
================================================
```

---

## Step 4: Run Examples

Now you can run examples:

```cmd
# Run full demo
wsl_run_demo.bat

# Run all examples
wsl_run_all_examples.bat

# Interactive menu
wsl_run_tests.bat
```

---

## 🚨 Troubleshooting

### Issue: "Clang P2996 not found"

**Solution**: You need to build the compiler first:

```cmd
cd wsl
wsl_setup_tools.bat
wsl_build_clang_p2996.bat
```

This is a ONE-TIME setup that takes 30-60 minutes.

### Issue: "WSL command not found"

**Solution**: Install WSL2:

```powershell
# In PowerShell as Administrator
wsl --install
# Restart computer
```

See [WSL2_QUICK_START.md](WSL2_QUICK_START.md) for details.

### Issue: Build fails with errors

**Solution**: Check environment:

```cmd
cd wsl
wsl_check_environment.bat
```

---

## 📋 Complete Setup Checklist

- [ ] WSL2 installed and running
- [ ] Basic tools installed (`wsl_setup_tools.bat`)
- [ ] Clang P2996 compiled (`wsl_build_clang_p2996.bat`)
- [ ] Project built (`wsl_rebuild_with_reflection.bat`)
- [ ] Examples running (`wsl_run_demo.bat`)

---

## ⚡ Quick Commands

### First Time (Complete Setup)

```cmd
cd wsl
wsl_diagnose.bat                    # Check what's needed
wsl_setup_tools.bat                 # Install tools
wsl_build_clang_p2996.bat          # Build compiler (30-60 min)
wsl_rebuild_with_reflection.bat    # Build project
wsl_run_demo.bat                    # Run demo
```

### After Setup (Daily Use)

```cmd
cd wsl
wsl_rebuild_with_reflection.bat    # Rebuild if code changed
wsl_run_tests.bat                   # Run tests
```

---

## 🎓 Understanding the Build Process

1. **Clang P2996**: Special compiler with C++26 reflection support
   - Built once from source
   - Takes 30-60 minutes
   - Stored in `~/clang-p2996-install/`

2. **Project Build**: Compiles your code
   - Uses Clang P2996
   - Takes 1-2 minutes
   - Creates `build_cpp26/bin/` with executables

3. **Running Examples**: Executes compiled programs
   - Requires `LD_LIBRARY_PATH` set (scripts handle this)
   - Instant execution

---

## 💡 Pro Tips

1. **First time?** Budget 1-2 hours for complete setup
2. **Already setup?** Just run `wsl_rebuild_with_reflection.bat`
3. **Quick test?** Use `wsl_diagnose.bat` to check status
4. **Need help?** See [WSL2_QUICK_START.md](WSL2_QUICK_START.md)

---

**Ready to start?** Run `wsl_diagnose.bat` to see your current status!
