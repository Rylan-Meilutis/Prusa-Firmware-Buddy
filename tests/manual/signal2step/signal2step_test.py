#!/usr/bin/env python3
"""
Visualizes signal2step conversion results.
Reconstructs position from steps and compares to commanded position.
"""

import numpy as np
import matplotlib.pyplot as plt
import sys

# Read metadata
metadata = {}
line = sys.stdin.readline()
while line.startswith('#') and '=' in line:
    key_value = line[1:].strip().split('=')
    if len(key_value) == 2:
        key = key_value[0].strip()
        try:
            value = float(key_value[1].strip())
            metadata[key] = value
        except ValueError:
            pass
    line = sys.stdin.readline()

sampling_freq = metadata['SAMPLING_FREQ']
mm_per_step = metadata['MM_PER_STEP']
duration = metadata['DURATION']

# Skip to positions section
while line.strip() != '# POSITIONS':
    line = sys.stdin.readline()
    if not line:
        print("Error: Could not find # POSITIONS section", file=sys.stderr)
        sys.exit(1)

# Read positions CSV header
positions_header = sys.stdin.readline()

# Read positions data
positions_data = []
line = sys.stdin.readline()
while line.strip() and not line.startswith('#'):
    positions_data.append([float(x) for x in line.strip().split(',')])
    line = sys.stdin.readline()

if not positions_data:
    print("Error: No position data found", file=sys.stderr)
    sys.exit(1)

positions = np.array(positions_data)

# Skip to STEPS section
while line.strip() != '# STEPS':
    line = sys.stdin.readline()
    if not line:
        print("Error: Could not find # STEPS section", file=sys.stderr)
        sys.exit(1)

# Read steps CSV
steps_header = sys.stdin.readline()
steps_data = []
for line in sys.stdin:
    if line.strip():
        parts = line.strip().split(',')
        sample = int(parts[0])
        time = float(parts[1])
        axis = parts[2]
        direction = int(parts[3])
        steps_data.append([sample, time, axis, direction])

# Extract commanded position data
time_cmd = positions[:, 1]
pos_cmd = {
    'X': positions[:, 2],
    'Y': positions[:, 3],
    'Z': positions[:, 4],
    'E': positions[:, 5]
}
vel_cmd = {
    'X': positions[:, 6],
    'Y': positions[:, 7],
    'Z': positions[:, 8],
    'E': positions[:, 9]
}

# Organize steps by axis
steps_by_axis = {'X': [], 'Y': [], 'Z': [], 'E': []}
for step in steps_data:
    axis = step[2]
    if axis in steps_by_axis:
        steps_by_axis[axis].append(step)


# Reconstruct position from steps for each axis
def reconstruct_position_from_steps(steps, mm_per_step, max_time):
    """Reconstruct position trace from step events"""
    if not steps:
        return np.array([0, max_time]), np.array([0, 0])

    # Sort steps by time
    steps_sorted = sorted(steps, key=lambda s: s[1])

    # Build position trace
    times = [0]
    positions = [0]
    current_pos = 0

    for step in steps_sorted:
        step_time = step[1]
        direction = step[3]

        # Add point just before step
        times.append(step_time)
        positions.append(current_pos)

        # Update position
        current_pos += mm_per_step if direction > 0 else -mm_per_step

        # Add point just after step
        times.append(step_time)
        positions.append(current_pos)

    # Extend to end of signal
    times.append(max_time)
    positions.append(current_pos)

    return np.array(times), np.array(positions)


# Reconstruct positions from steps
pos_recon = {}
time_recon = {}
for axis_name in ['X', 'Y', 'Z', 'E']:
    time_recon[axis_name], pos_recon[
        axis_name] = reconstruct_position_from_steps(steps_by_axis[axis_name],
                                                     mm_per_step, duration)

# Create figure with subplots
fig = plt.figure(figsize=(18, 12))
gs = fig.add_gridspec(4, 3, hspace=0.35, wspace=0.35)

axes_data = [('X', 'blue'), ('Y', 'green'), ('Z', 'orange'), ('E', 'red')]

for i, (axis_name, color) in enumerate(axes_data):
    # Position comparison plot
    ax_pos = fig.add_subplot(gs[i, 0])

    # Plot commanded position
    ax_pos.plot(time_cmd,
                pos_cmd[axis_name],
                color=color,
                linewidth=1.5,
                alpha=0.7,
                label='Commanded',
                zorder=2)

    # Plot reconstructed position from steps
    ax_pos.plot(time_recon[axis_name],
                pos_recon[axis_name],
                color='black',
                linewidth=1,
                linestyle='--',
                alpha=0.8,
                label='From Steps',
                zorder=3)

    # Mark individual steps
    if steps_by_axis[axis_name]:
        step_times = [s[1] for s in steps_by_axis[axis_name]]
        step_positions = np.interp(step_times, time_recon[axis_name],
                                   pos_recon[axis_name])
        step_dirs = [s[3] for s in steps_by_axis[axis_name]]

        forward_mask = np.array(step_dirs) > 0
        backward_mask = np.array(step_dirs) < 0

        if np.any(forward_mask):
            ax_pos.scatter(np.array(step_times)[forward_mask],
                           np.array(step_positions)[forward_mask],
                           color='darkgreen',
                           marker='^',
                           s=15,
                           alpha=0.5,
                           label='Step +',
                           zorder=4)
        if np.any(backward_mask):
            ax_pos.scatter(np.array(step_times)[backward_mask],
                           np.array(step_positions)[backward_mask],
                           color='darkred',
                           marker='v',
                           s=15,
                           alpha=0.5,
                           label='Step -',
                           zorder=4)

    ax_pos.set_xlabel('Time (s)')
    ax_pos.set_ylabel(f'{axis_name} Position (mm)')
    ax_pos.set_title(f'{axis_name} Axis - Position Comparison')
    ax_pos.legend(loc='best', fontsize=8)
    ax_pos.grid(alpha=0.3)

    # Position error plot
    ax_err = fig.add_subplot(gs[i, 1])

    # Interpolate reconstructed position onto commanded time grid
    pos_recon_interp = np.interp(time_cmd, time_recon[axis_name],
                                 pos_recon[axis_name])
    error = pos_cmd[axis_name] - pos_recon_interp

    ax_err.plot(
        time_cmd,
        error * 1000,  # Convert to microns
        color=color,
        linewidth=1)
    ax_err.axhline(y=0,
                   color='black',
                   linestyle='--',
                   linewidth=0.5,
                   alpha=0.5)
    ax_err.fill_between(time_cmd, 0, error * 1000, alpha=0.3, color=color)

    # Calculate RMS error
    rms_error = np.sqrt(np.mean(error**2))
    max_error = np.max(np.abs(error))

    ax_err.set_xlabel('Time (s)')
    ax_err.set_ylabel(f'Position Error (µm)')
    ax_err.set_title(
        f'{axis_name} - Error (RMS: {rms_error*1000:.2f}µm, Max: {max_error*1000:.2f}µm)'
    )
    ax_err.grid(alpha=0.3)

    # Velocity comparison plot
    ax_vel = fig.add_subplot(gs[i, 2])

    # Plot commanded velocity
    ax_vel.plot(time_cmd,
                vel_cmd[axis_name],
                color=color,
                linewidth=1.5,
                alpha=0.7,
                label='Commanded')

    # Calculate velocity from step events
    if len(steps_by_axis[axis_name]) > 1:
        steps_sorted = sorted(steps_by_axis[axis_name], key=lambda s: s[1])

        # Compute velocity between consecutive steps
        vel_times = []
        vel_values = []

        for j in range(len(steps_sorted) - 1):
            t1 = steps_sorted[j][1]
            t2 = steps_sorted[j + 1][1]
            dir1 = steps_sorted[j][3]

            # Velocity is step_size / time_interval
            dt = t2 - t1
            if dt > 0:
                velocity = (mm_per_step /
                            dt) if dir1 > 0 else -(mm_per_step / dt)

                # Create step function: velocity is constant between steps
                vel_times.extend([t1, t2])
                vel_values.extend([velocity, velocity])

        if vel_times:
            ax_vel.plot(vel_times,
                        vel_values,
                        color='black',
                        linewidth=1,
                        linestyle='--',
                        alpha=0.8,
                        label='From Steps',
                        drawstyle='steps-post')

    ax_vel.set_xlabel('Time (s)')
    ax_vel.set_ylabel(f'{axis_name} Velocity (mm/s)')
    ax_vel.set_title(f'{axis_name} Axis - Velocity')
    ax_vel.legend(loc='best', fontsize=8)
    ax_vel.grid(alpha=0.3)

# Add summary text
summary_text = (
    f"Sampling: {sampling_freq:.0f} Hz | "
    f"Resolution: {mm_per_step*1000:.3f} µm/step ({1/mm_per_step:.0f} steps/mm) | "
    f"Duration: {duration:.2f}s\n"
    f"Total steps: X={len(steps_by_axis['X'])}, "
    f"Y={len(steps_by_axis['Y'])}, "
    f"Z={len(steps_by_axis['Z'])}, "
    f"E={len(steps_by_axis['E'])}")
fig.suptitle('Signal2Step Conversion Test - Position Reconstruction',
             fontsize=14,
             fontweight='bold')
fig.text(0.5,
         0.98,
         summary_text,
         ha='center',
         va='top',
         fontsize=10,
         bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.3))

fig.subplots_adjust(left=0.05, right=0.98, bottom=0.06, top=0.90)

# Check if we should save to file or show interactively
if len(sys.argv) > 1:
    output_file = sys.argv[1]
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"Plot saved to {output_file}", file=sys.stderr)
else:
    plt.show()
