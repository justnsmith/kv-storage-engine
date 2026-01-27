package commands

import (
	"fmt"
	"os"

	"github.com/justnsmith/distributed-kv-storage/cli/internal/config"
	"github.com/spf13/cobra"
)

var configCmd = &cobra.Command{
	Use:   "config",
	Short: "Manage CLI configuration",
	Long:  `View and manage the CLI configuration.`,
}

var configShowCmd = &cobra.Command{
	Use:   "show",
	Short: "Show current configuration",
	Long:  `Display the current configuration, including values from config file, environment variables, and command-line flags.`,
	Run: func(cmd *cobra.Command, args []string) {
		cfgMap := map[string]interface{}{
			"server.host":       cfg.Server.Host,
			"server.port":       cfg.Server.Port,
			"server.timeout_ms": cfg.Server.TimeoutMs,
			"output.format":     cfg.Output.Format,
			"output.color":      cfg.Output.Color,
		}

		formatter.PrintConfig(cfgMap)
	},
}

var configInitCmd = &cobra.Command{
	Use:   "init",
	Short: "Initialize default configuration",
	Long:  `Create a default configuration file at ~/.kv/config.yaml`,
	Run: func(cmd *cobra.Command, args []string) {
		if err := config.CreateDefaultConfig(); err != nil {
			formatter.PrintError(fmt.Sprintf("failed to create config: %v", err))
			os.Exit(1)
		}

		home, _ := os.UserHomeDir()
		formatter.PrintSuccess(fmt.Sprintf("Created default config at %s/.kv/config.yaml", home))
	},
}

func init() {
	configCmd.AddCommand(configShowCmd)
	configCmd.AddCommand(configInitCmd)
}
