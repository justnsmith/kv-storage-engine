package commands

import (
	"fmt"
	"os"

	"github.com/justnsmith/kv-storage-engine/cli/internal/config"
	"github.com/justnsmith/kv-storage-engine/cli/internal/output"
	"github.com/spf13/cobra"
)

var (
	cfg       *config.Config
	formatter *output.Formatter

	// Global flags
	host    string
	port    int
	format  string
	noColor bool
)

// rootCmd represents the base command
var rootCmd = &cobra.Command{
	Use:   "kv",
	Short: "KV Storage Engine CLI",
	Long: `A command-line interface for interacting with the KV Storage Engine.

The KV Storage Engine is a high-performance, LSM-tree based key-value store
built in C++. This CLI provides a simple interface for basic operations.`,
	PersistentPreRun: func(cmd *cobra.Command, args []string) {
		// Load config
		var err error
		cfg, err = config.Load()
		if err != nil {
			fmt.Fprintf(os.Stderr, "Warning: failed to load config: %v\n", err)
			// Use defaults
			cfg = &config.Config{
				Server: config.ServerConfig{
					Host:      config.DefaultHost,
					Port:      config.DefaultPort,
					TimeoutMs: config.DefaultTimeoutMs,
				},
				Output: config.OutputConfig{
					Format: config.DefaultFormat,
					Color:  config.DefaultColor,
				},
			}
		}

		// Override with command-line flags if provided
		if cmd.Flags().Changed("host") {
			cfg.Server.Host = host
		}
		if cmd.Flags().Changed("port") {
			cfg.Server.Port = port
		}
		if cmd.Flags().Changed("format") {
			cfg.Output.Format = format
		}
		if cmd.Flags().Changed("no-color") {
			cfg.Output.Color = !noColor
		}

		// Initialize formatter
		formatter = output.NewFormatter(cfg.Output.Format, cfg.Output.Color)
	},
}

// Execute runs the root command
func Execute() {
	if err := rootCmd.Execute(); err != nil {
		os.Exit(1)
	}
}

func init() {
	// Global flags
	rootCmd.PersistentFlags().StringVar(&host, "host", "", "Server host (default: from config or 127.0.0.1)")
	rootCmd.PersistentFlags().IntVar(&port, "port", 0, "Server port (default: from config or 9000)")
	rootCmd.PersistentFlags().StringVar(&format, "format", "", "Output format: text or json (default: text)")
	rootCmd.PersistentFlags().BoolVar(&noColor, "no-color", false, "Disable colored output")

	// Add subcommands
	rootCmd.AddCommand(getCmd)
	rootCmd.AddCommand(setCmd)
	rootCmd.AddCommand(deleteCmd)
	rootCmd.AddCommand(pingCmd)
	rootCmd.AddCommand(configCmd)
}
