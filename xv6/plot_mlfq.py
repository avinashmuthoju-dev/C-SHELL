import re
import matplotlib.pyplot as plt

data = []

# ---------------------------------------------------------
# Read MLFQ trace
# ---------------------------------------------------------
with open("mlfq_trace.txt", "r") as f:
    for line in f:
        match = re.search(
            r"TRACE tick=(\d+) pid=(\d+) q=(\d+)",
            line
        )

        if match:
            tick = int(match.group(1))
            pid = int(match.group(2))
            queue = int(match.group(3))

            data.append((tick, pid, queue))


# ---------------------------------------------------------
# Check data
# ---------------------------------------------------------
if not data:
    print("ERROR: No MLFQ trace data found.")
    print("Make sure mlfq_trace.txt contains lines like:")
    print("TRACE tick=1250 pid=4 q=0")
    exit()


# ---------------------------------------------------------
# Scheduler test starts at tick 1249
# ---------------------------------------------------------
start_tick = 1249


# Convert global ticks -> elapsed ticks
elapsed_data = [
    (tick - start_tick, pid, queue)
    for tick, pid, queue in data
]


# Get all PIDs
pids = sorted(
    set(pid for tick, pid, queue in elapsed_data)
)


# ---------------------------------------------------------
# Create figure
# ---------------------------------------------------------
plt.figure(figsize=(13, 8))


# ---------------------------------------------------------
# Plot each PID as a connected queue trajectory
# ---------------------------------------------------------
for pid in pids:

    x = []
    y = []

    for tick, process, queue in elapsed_data:

        if process == pid:
            x.append(tick)
            y.append(queue)

    # Connect queue states
    plt.plot(
        x,
        y,
        marker="o",
        markersize=3,
        linewidth=1.2,
        label=f"PID {pid}"
    )


# ---------------------------------------------------------
# Maximum elapsed time
# ---------------------------------------------------------
max_elapsed = max(
    tick for tick, pid, queue in elapsed_data
)


# ---------------------------------------------------------
# Mark actual priority boosts
#
# Kernel:
#     if (ticks % 48 == 0)
#
# Starting global tick = 1249
#
# Boosts:
#     1296 -> elapsed 47
#     1344 -> elapsed 95
# ---------------------------------------------------------
first_boost = ((start_tick // 48) + 1) * 48

for global_tick in range(
    first_boost,
    start_tick + max_elapsed + 1,
    48
):

    elapsed = global_tick - start_tick

    plt.axvline(
        x=elapsed,
        linestyle="--",
        linewidth=1.2
    )

    plt.text(
        elapsed,
        3.18,
        f"Boost\n{elapsed}",
        rotation=90,
        verticalalignment="top",
        horizontalalignment="right",
        fontsize=8
    )


# ---------------------------------------------------------
# Axis labels
# ---------------------------------------------------------
plt.xlabel(
    "Time elapsed since scheduler test start (ticks)"
)

plt.ylabel("Queue ID")


# Queue IDs
plt.yticks([0, 1, 2, 3])

plt.ylim(-0.5, 3.5)


# ---------------------------------------------------------
# Title
# ---------------------------------------------------------
plt.title(
    "MLFQ Scheduling Timeline"
)


# ---------------------------------------------------------
# Grid
# ---------------------------------------------------------
plt.grid(
    True,
    alpha=0.3
)


# ---------------------------------------------------------
# Legend
# ---------------------------------------------------------
plt.legend(
    title="Process",
    loc="upper right"
)


# ---------------------------------------------------------
# Watermark
# ---------------------------------------------------------
plt.text(
    0.99,
    0.01,
    "avinash.muthoju",
    transform=plt.gca().transAxes,
    horizontalalignment="right",
    verticalalignment="bottom",
    fontsize=9
)


# ---------------------------------------------------------
# Save figure
# ---------------------------------------------------------
plt.tight_layout()

plt.savefig(
    "mlfq_timeline.png",
    dpi=300,
    bbox_inches="tight"
)


# ---------------------------------------------------------
# Display
# ---------------------------------------------------------
plt.show()

print("Plot generated successfully:")
print("mlfq_timeline.png")