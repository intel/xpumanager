/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Example application demonstrating GPU monitoring output using TableBuilder
 */

#include "table_builder.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

void exampleBasicGpuInfo()
{
	std::cout << "Example 1: Basic GPU Information\n";
	std::cout << "---------------------------------------------------------------------------\n";

	TableBuilder()
		.addColumn("GPU", 5, Align::Right)
		.addColumn("Name", 25, Align::Left)
		.addColumn("Temp", 6, Align::Right)
		.addColumn("Power", 10, Align::Right)
		.addColumn("Memory", 15, Align::Right)
		.addRow(0, "Intel(R) Graphics [0xe221]", "45°C", "75W / 300W", "512MB / 128GB")
		.addRow(1, "Intel(R) Graphics [0xe221]", "43°C", "72W / 300W", "256MB / 128GB")
		.addRow(2, "Intel(R) Graphics [0xe221]", "47°C", "78W / 300W", "1024MB / 128GB")
		.addRow(3, "Intel(R) Graphics [0xe221]", "44°C", "74W / 300W", "128MB / 128GB")
		.enableAutoSizing()
		.print();

	std::cout << "\n";
}

void exampleProcessInfo()
{
	std::cout << "Example 2: Process Information (Auto-sized columns)\n";
	std::cout << "---------------------------------------------------------------------------\n";

	TableBuilder()
		.addColumn("GPU", Align::Center)
		.addColumn("PID", Align::Right)
		.addColumn("Type", Align::Center)
		.addColumn("Process Name", Align::Left)
		.addColumn("GPU Memory Usage", Align::Right)
		.addRow(0, 12345, "C", "python3 train_model.py", "4096 MiB")
		.addRow(0, 12346, "C", "python3 inference.py", "2048 MiB")
		.addRow(1, 23456, "C", "pytorch_worker", "8192 MiB")
		.addRow(2, 34567, "G", "Xorg", "512 MiB")
		.addRow(3, 45678, "C", "tensorflow_server", "6144 MiB")
		.enableAutoSizing()
		.print();

	std::cout << "\n";
}

void exampleComprehensiveStatus()
{
	std::cout << "Example 3: Comprehensive GPU Status\n";
	std::cout << "---------------------------------------------------------------------------\n";

	TableBuilder()
		.addColumn("Metric", 20, Align::Left)
		.addColumn("GPU 0", 15, Align::Right)
		.addColumn("GPU 1", 15, Align::Right)
		.addColumn("GPU 2", 15, Align::Right)
		.addColumn("GPU 3", 15, Align::Right)
		.addRow("Temperature", "45°C", "43°C", "47°C", "44°C")
		.addRow("Power Draw", "75 W", "72 W", "78 W", "74 W")
		.addRow("GPU Utilization", "95%", "87%", "92%", "89%")
		.addRow("Memory Used", "512 MiB", "256 MiB", "1024 MiB", "128 MiB")
		.addRow("Memory Total", "16 GiB", "16 GiB", "16 GiB", "16 GiB")
		.addRow("Clock Speed", "1530 MHz", "1455 MHz", "1545 MHz", "1500 MHz")
		.addRow("Fan Speed", "45%", "42%", "48%", "43%")
		.enableAutoSizing()
		.print();

	std::cout << "\n";
}

void exampleJsonOutput()
{
	std::cout << "Example 4: JSON Output Format\n";
	std::cout << "---------------------------------------------------------------------------\n";

	auto jsonOutput = TableBuilder()
						  .addColumn("GPU", 5, Align::Right)
						  .addColumn("Name", 25, Align::Left)
						  .addColumn("Temperature", 12, Align::Right)
						  .addColumn("Power", 12, Align::Right)
						  .addColumn("Memory", 12, Align::Right)
						  .addRow(0, "Intel(R) Graphics [0xe221]", "45°C", "75W", "512MB")
						  .addRow(1, "Intel(R) Graphics [0xe221]", "43°C", "72W", "256MB")
						  .addRow(2, "Intel(R) Graphics [0xe221]", "47°C", "78W", "1024MB")
						  .addRow(3, "Intel(R) Graphics [0xe221]", "44°C", "74W", "128MB")
						  .asJson();

	std::cout << jsonOutput << "\n\n";
}

void exampleFilteredData()
{
	std::cout << "Example 6: High Temperature GPUs (Temperature > 45°C)\n";
	std::cout << "---------------------------------------------------------------------------\n";

	TableBuilder tb;
	tb.addColumn("GPU", 5, Align::Right)
		.addColumn("Name", 25, Align::Left)
		.addColumn("Temperature", 12, Align::Right)
		.addColumn("Power", 12, Align::Right)
		.addRow(0, "Intel(R) Graphics [0xe221]", "45°C", "75W")
		.addRow(1, "Intel(R) Graphics [0xe221]", "43°C", "72W")
		.addRow(2, "Intel(R) Graphics [0xe221]", "47°C", "78W")
		.addRow(3, "Intel(R) Graphics [0xe221]", "44°C", "74W")
		.enableAutoSizing();

	// Filter to show only GPUs with temperature >= 45°C
	tb.filterRows([](const TableBuilder::Row &row) -> bool {
		if (row.cells.size() >= 3) {
			const auto &tempStr = row.cells[2]; // Temperature column
			// Simple check: if starts with "47" or "45"
			return tempStr.rfind("47", 0) == 0 || tempStr.rfind("45", 0) == 0;
		}
		return false;
	});

	tb.print();

	std::cout << "\n";
}

void exampleEmptyTable()
{
	std::cout << "Example 7: Empty Table (No GPUs Found)\n";
	std::cout << "---------------------------------------------------------------------------\n";

	TableBuilder()
		.addColumn("GPU", 5, Align::Right)
		.addColumn("Name", 25, Align::Left)
		.addColumn("Temperature", 12, Align::Right)
		.addColumn("Power", 12, Align::Right)
		.enableAutoSizing()
		.print();

	std::cout << "\n";
}

void exampleTableStatistics()
{
	std::cout << "Example 8: Table Statistics\n";
	std::cout << "---------------------------------------------------------------------------\n";

	TableBuilder statsTable;
	statsTable.addColumn("GPU", 5, Align::Right)
		.addColumn("Name", 25, Align::Left)
		.addColumn("Status", 12, Align::Center)
		.addRow(0, "Intel(R) Graphics [0xe221]", "Active")
		.addRow(1, "Intel(R) Graphics [0xe221]", "Active")
		.addRow(2, "", "Idle") // Empty cell example
		.addRow(3, "Intel(R) Graphics [0xe221]", "Active");

	auto stats = statsTable.getStats();
	std::cout << "Total Cells: " << stats.totalCells << "\n";
	std::cout << "Empty Cells: " << stats.emptyCells << "\n";
	std::cout << "Total Characters: " << stats.totalCharacters << "\n";
	std::cout << "Fill Ratio: " << (stats.fillRatio * 100.0) << "%\n";

	std::cout << "\n";
	statsTable.enableAutoSizing().print();

	std::cout << "\n";
}

void exampleGpuMonitoringStyle()
{
	std::cout << "Example 9: Full GPU Monitoring Layout\n";
	std::cout << "---------------------------------------------------------------------------\n";

	TableBuilder table;

	// Add columns for GPU monitoring layout
	table.addColumn("GPU", 5, Align::Center)
		.addColumn("Name", 22, Align::Left)
		.addColumn("Persistence-M", 15, Align::Center)
		.addColumn("Bus-Id", 20, Align::Center)
		.addColumn("Disp.A", 8, Align::Center)
		.addColumn("Volatile Uncorr. ECC", 22, Align::Center)
		.enableAutoSizing();

	// Add title row (spans all columns)
	table.addSpanRow("Intel GPU Monitor    Driver Version: 1.3.28119    Level Zero Version: 1.16", BorderStyle::Heavy);

	// Add separator
	table.addSeparator(BorderStyle::Heavy);

	// Add multi-line row for GPU 0
	table.addMultiLineRow({
		{"0", "0"},											  // GPU column (2 lines)
		{"Intel(R) Graphics [0xe221]", "Intel(R) Graphics [0xe221]", ""}, // Name
		{"Off", "Off", ""},									  // Persistence-M
		{"00000000:00:1E.0", "00000000:00:1E.0", ""},		  // Bus-Id
		{"Off", "Off", ""},									  // Disp.A
		{"0", "0", "N/A"}									  // Volatile Uncorr. ECC
	});

	table.addMultiLineRow({{"", ""},
						   {"Fan  Temp  Perf  Pwr:Usage/Cap", "N/A   32C    P0    38W / 300W"},
						   {"Memory-Usage", "0MiB / 32510MiB"},
						   {"GPU-Util", "0%"},
						   {"Compute M.", "Default"},
						   {"MIG M.", "N/A"}});

	// Add separator between GPUs
	table.addSeparator(BorderStyle::Normal);

	// Add GPU 1
	table.addMultiLineRow({{"1", "1"},
						   {"Intel(R) Graphics [0xe221]", "Intel(R) Graphics [0xe221]", ""},
						   {"Off", "Off", ""},
						   {"00000000:00:1F.0", "00000000:00:1F.0", ""},
						   {"Off", "Off", ""},
						   {"0", "0", "N/A"}});

	table.addMultiLineRow({{"", ""},
						   {"Fan  Temp  Perf  Pwr:Usage/Cap", "N/A   35C    P0    42W / 300W"},
						   {"Memory-Usage", "1024MiB / 32510MiB"},
						   {"GPU-Util", "12%"},
						   {"Compute M.", "Default"},
						   {"MIG M.", "N/A"}});

	// Add bottom separator
	table.addSeparator(BorderStyle::Heavy);

	// Add process information section
	table.addSpanRow("Processes:", BorderStyle::Normal);
	std::cout << "\n";
}

void exampleLiveMonitoring()
{
	std::cout << "Example 10: Live GPU Monitoring (Streaming Updates)\n";
	std::cout << "---------------------------------------------------------------------------\n";
	std::cout << "This example demonstrates live table updates. Press Ctrl+C to stop.\n";
	std::cout << "Starting in 2 seconds...\n\n";

	std::this_thread::sleep_for(std::chrono::seconds(2));

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> tempDist(40, 85);
	std::uniform_int_distribution<> powerDist(50, 150);
	std::uniform_int_distribution<> utilDist(0, 100);
	std::uniform_int_distribution<> memDist(100, 8000);

	TableBuilder table;
	table.addColumn("GPU", 5, Align::Center)
		.addColumn("Name", 20, Align::Left)
		.addColumn("Temp", 8, Align::Right)
		.addColumn("Power", 12, Align::Right)
		.addColumn("GPU%", 8, Align::Right)
		.addColumn("Memory", 15, Align::Right)
		.enableAutoSizing()
		.startStreaming();

	const int totalIterations = 30;
	for (int i = 0; i < totalIterations; ++i) {
		// Clear existing rows and add updated data
		table.clearRows();

		// Add 4 GPUs with randomly changing metrics
		for (int gpu = 0; gpu < 4; ++gpu) {
			int temp = tempDist(gen);
			int power = powerDist(gen);
			int util = utilDist(gen);
			int mem = memDist(gen);

			table.addRow(gpu, "Intel(R) Graphics [0xe221]", std::to_string(temp) + "°C", std::to_string(power) + "W / 300W",
						 std::to_string(util) + "%", std::to_string(mem) + "MB / 128GB");
		}

		// Add a status line showing iteration
		table.addSeparator(BorderStyle::Normal);
		table.addSpanRow(std::string("Update: ") + std::to_string(i + 1) + "/" + std::to_string(totalIterations) +
							 " | Time: " + std::to_string((i + 1) * 0.5) + "s",
						 BorderStyle::None);

		// Update the display in place
		table.printStreaming();
		
		// Pause to show the live update effect
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	table.endStreaming();

	std::cout << "\nLive monitoring completed!\n\n";
}

void exampleIncrementalRows()
{
	std::cout << "Example 11: Incremental Row Addition (Streaming)\n";
	std::cout << "---------------------------------------------------------------------------\n";
	std::cout << "Demonstrating adding rows one at a time with live updates.\n\n";

	std::this_thread::sleep_for(std::chrono::seconds(1));

	TableBuilder table;
	table.addColumn("Step", 6, Align::Right)
		.addColumn("Operation", 30, Align::Left)
		.addColumn("Status", 12, Align::Center)
		.addColumn("Time", 10, Align::Right)
		.enableAutoSizing()
		.startStreaming();

	// Simulate processing steps being added incrementally
	const std::vector<std::string> operations = {
		"Initializing GPUs",	 "Loading models",		  "Allocating memory",		"Starting training",
		"Processing batch 1/10", "Processing batch 2/10", "Processing batch 3/10",	"Processing batch 4/10",
		"Processing batch 5/10", "Validation check",	  "Processing batch 6/10",	"Processing batch 7/10",
		"Processing batch 8/10", "Processing batch 9/10", "Processing batch 10/10", "Saving checkpoint",
		"Training complete"};

	for (size_t i = 0; i < operations.size(); ++i) {
		table.addRow(i + 1, operations[i], "Running", std::to_string(static_cast<double>(i + 1) * 0.3) + "s");

		// Update display
		table.printStreaming();

		std::this_thread::sleep_for(std::chrono::milliseconds(300));

		if (!table.empty()) {
			table.setCell(table.rowCount() - 1, 2, "✓ Done");
			table.printStreaming();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	table.endStreaming();

	std::cout << "\nAll operations completed!\n\n";
}

int main()
{
	std::cout << "===========================================================================\n";
	std::cout << "GPU Monitoring Example using TableBuilder\n";
	std::cout << "===========================================================================\n\n";

	exampleBasicGpuInfo();
	exampleProcessInfo();
	exampleComprehensiveStatus();
	exampleJsonOutput();
	exampleFilteredData();
	exampleEmptyTable();
	exampleTableStatistics();
	exampleGpuMonitoringStyle();

	std::cout << "===========================================================================\n";
	std::cout << "Static examples completed! Starting streaming examples...\n";
	std::cout << "===========================================================================\n\n";

	exampleLiveMonitoring();
	exampleIncrementalRows();

	std::cout << "===========================================================================\n";
	std::cout << "All examples completed successfully!\n";
	std::cout << "===========================================================================\n";

	return 0;
}
