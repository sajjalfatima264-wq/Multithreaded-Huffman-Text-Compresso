import matplotlib.pyplot as plt

# Thread counts
threads = [1, 2, 4, 8]

# Corresponding completion times (file read excluded)
completion_times = [0.003211, 0.002325, 0.002212, 0.002739]

# Create the plot
plt.figure(figsize=(8,5))
plt.plot(threads, completion_times, marker='o', linestyle='-', color='b', label='Completion Time')

# Labels and title
plt.xlabel('Number of Threads')
plt.ylabel('Completion Time (s)')
plt.title('Completion Time vs Number of Threads')
plt.xticks(threads)  # Show all thread counts on x-axis
plt.grid(True)
plt.legend()

# Show the plot
plt.show()

