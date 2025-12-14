# SDL3 Solar System Simulator
*A real-time, interactive pseudo-3D planetary system written in C using SDL3*

![Solar System Overview](screenshots/solar_system_overview.png)

---

## Overview

The **SDL3 Solar System Simulator** is a real-time graphical application that visualizes a planetary system using a custom pseudo-3D projection.  
It features orbiting planets, moons, an asteroid belt, star field background, camera controls, and an integrated UI for runtime planet management.

This project is intended for educational use, graphics experimentation, and portfolio demonstration.

---

## Features

- Pseudo-3D camera with depth-based scaling
- Real-time orbital simulation
- Mouse-driven camera rotation, panning, and zoom
- Planetary occlusion behind the sun
- Moons orbiting parent planets
- Asteroid belt rendering
- Parallax star background
- Built-in UI (no external GUI libraries)
- Runtime planet creation and deletion
- Persistent configuration via text file

---

## Screenshots

### Main Simulation View
![Main View](screenshots/solar_system_overview.png)

### Add Planet Interface
![Add Planet](screenshots/add_planet.png)

### Remove Planet Interface
![Remove Planet](screenshots/remove_planet.png)

### Edge-On Camera View
![Edge View](screenshots/edge_view.png)

### Wide Orbit Perspective
![Wide Orbit](screenshots/wide_orbit.png)

---

## Controls

### Camera
- **Left Mouse Button + Drag** — Rotate camera
- **Right Mouse Button + Drag** — Pan camera
- **Mouse Wheel** — Zoom in / out

### Interface
- **ADD PLANET** — Open planet creation panel
- **REMOVE PLANET** — Open planet removal panel
- **TAB** — Switch input fields
- **ENTER** — Save planet
- **ESC** — Cancel / close panels

---

## Planet Configuration (`planets.txt`)

Planets are loaded from and saved to a plain text file named:

```
planets.txt
```

### Format

```
<Name> <OrbitRadius> <AngularSpeed> <Radius> <R> <G> <B>
```

### Example

```
Earth 200 0.010 8 100 150 255
Mars 260 0.008 6 200 80 80
Jupiter 360 0.004 18 220 200 150
```

---

## Build Instructions

### Requirements
- SDL3
- GCC or Clang
- Math library

### Build (Linux / macOS)

```bash
gcc main.c -o solar_system `sdl3-config --cflags --libs` -lm
```

Ensure `planets.txt` is present in the executable directory.

---

## Project Structure

```
.
├── main.c
├── planets.txt
├── screenshots/
│   ├── solar_system_overview.png
│   ├── add_planet.png
│   ├── remove_planet.png
│   ├── edge_view.png
│   └── wide_orbit.png
└── README.md
```

---

## License

This project is provided for educational and demonstration purposes.  
Add a formal open-source license if distributing publicly.
