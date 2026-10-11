# Realistic lift visual refresh

## Scope

Change only the lift shafts and moving cars in the existing simulation. Preserve the setup screen, policy cards, passengers waiting on floors, explanations, metrics, chart, playback controls, responsive layout, C data flow, and all other behavior.

## Visual design

Each shaft becomes a narrow brushed-steel tower with dark guide rails, inset highlights, and a subtle recessed center. Each moving car becomes a framed metal cabin inspired by a modern observation lift: beveled casing, warm ceiling light, glass front, compact lift badge, and a direction indicator.

Riders remain visible at all times through the glass front. A service tick adds a warm light and slight frame glow rather than splitting the door panels. Movement, floor position, rider data, and accessibility titles continue to come from the existing frame renderer.

## Implementation and verification

Use only HTML and CSS changes local to the shaft markup and styling. Do not add external assets or libraries. Verify moving, idle, and service states at desktop and mobile widths, confirm multiple lifts still fit, and rerun the existing JavaScript and browser smoke checks.
