# Light simulation-game UI design

## Goal

Turn the lift scheduler comparison into a simple, polished teaching simulation. A user chooses a passenger count and a few building settings, starts a run, and watches people queue, enter lifts, and reach their destinations under FCFS and nearest-stop-first.

## Architecture

The existing C program remains the source of truth. A small local HTTP bridge accepts validated setup values, creates a temporary scenario, runs the C executable once per scheduling policy, and returns the exported results. The browser renders and animates those results; it does not reimplement scheduling.

The launcher builds the C program, starts the bridge, and prints the local address. Existing terminal commands, scenarios, exports, and tests remain available.

## Interface

The visual direction is a bright, restrained management game: warm white canvas, blue structural outlines, friendly pixel-inspired people, small status signs, and minimal shadows. It borrows the clarity and character of the references without copying their dense mobile-game chrome.

The first view contains a compact setup card for passengers, floors, lifts, capacity, and seed. Starting creates one deterministic passenger population and runs both algorithms against it. The comparison view shows two buildings side by side, visible floor queues, moving lift cars, opening doors, passenger boarding and delivery, and one plain-language decision message per policy.

Controls are limited to pause/play, restart, speed, timeline, and new simulation. The primary results are served passengers, average wait, utilization, and completion time. Advanced operating-system details remain in a collapsible learning panel.

## Data flow and safety

All input is numeric, range checked in both the browser and server, and passed to the simulator without shell interpolation. Each request uses a private temporary directory which is deleted after parsing. The server only serves files inside the project and exposes a single simulation endpoint.

## Error handling

Invalid settings are shown beside the setup controls. Build failures, malformed simulator output, and unavailable server responses produce a friendly error panel with the exact recovery command. The interface never silently substitutes browser-generated results.

## Verification

Existing C unit, smoke, and integration tests must continue to pass. New bridge tests cover validation, deterministic output, matching passenger sets, and both policies. Browser checks cover setup, animation controls, responsive layout, and error states. A final rendered screenshot is inspected at desktop and narrow widths.
