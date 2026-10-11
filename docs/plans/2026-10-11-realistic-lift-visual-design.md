# Realistic lift visual refresh

## Scope

Change only the lift shafts and moving cars in the existing simulation. Preserve the setup screen, policy cards, passengers waiting on floors, explanations, metrics, chart, playback controls, responsive layout, C data flow, and all other behavior.

## Visual design

Each shaft becomes a recessed mechanical channel with dark guide rails, cable lines, inset highlights, and a segmented center wall. Each moving car becomes a visibly open metal cabin: brushed header, compact illuminated lift badge and direction indicator, thick side posts, warm ceiling spots, paneled back wall, handrail, and a perspective floor.

Riders remain visible at all times inside the open cabin and stand on its floor. A service tick strengthens the warm light and frame glow. Movement, floor position, rider data, and accessibility titles continue to come from the existing frame renderer.

## Implementation and verification

Use only HTML and CSS changes local to the shaft markup and styling. Do not add external assets or libraries. Verify moving, idle, and service states at desktop and mobile widths, confirm multiple lifts still fit, and rerun the existing JavaScript and browser smoke checks.
