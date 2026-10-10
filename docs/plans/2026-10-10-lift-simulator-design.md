# Smart lift controller design

The project is a Linux C11 multi-process lift simulation built around one deterministic pure engine. The pure engine owns arrivals, assignments, stop selection, actions, frames, events and metrics. Its only integration points are boarding and exit callbacks; tests use integer capacity while the live controller uses passenger-process semaphore handshakes.

The controller is the only clock owner, writer and terminal renderer. It forks passengers at their exact arrival tick, reads a fixed number of atomic pipe requests, sorts them by ID and then advances the same engine tested by the golden scenarios. Shared anonymous memory holds passenger commands, lift capacity semaphores and the readers-writers display board.

The web visualization is a self-contained graphite-and-brass elevator-lobby interface. It loads two generated JavaScript data files through script tags, synchronizes FCFS and nearest playback, animates cars and doors, exposes keyboard and scrub controls, charts queue depth, and presents the final policy comparison without external dependencies.

Implementation follows the ten supplied stages in two commits: stages 1–5 establish parsing, pure scheduling, terminal rendering, live processes and the board; stages 6–10 complete lifecycle demonstrations, export, web visualization, scripts, generated scenarios and documentation.
