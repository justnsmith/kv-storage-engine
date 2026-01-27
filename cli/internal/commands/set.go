package commands

import (
	"fmt"
	"os"

	"github.com/justnsmith/distributed-kv-storage/cli/internal/client"
	"github.com/spf13/cobra"
)

var setCmd = &cobra.Command{
	Use:   "set <key> <value>",
	Short: "Set the value of a key",
	Long: `Store a key-value pair in the storage engine.

The value can contain spaces and special characters.

Examples:
  kv set user:1 '{"name":"Justin","age":25}'
  kv set counter 42
  kv set message "Hello, World!"`,
	Args: cobra.ExactArgs(2),
	Run: func(cmd *cobra.Command, args []string) {
		key := args[0]
		value := args[1]

		// Create client
		c := client.NewClient(cfg.Server.Host, cfg.Server.Port, cfg.Server.GetTimeout())

		// Connect
		if err := c.Connect(); err != nil {
			formatter.PrintError(fmt.Sprintf("failed to connect: %v", err))
			os.Exit(1)
		}
		defer func() { _ = c.Close() }()

		// Execute SET
		resp, err := c.Set(key, value)
		if err != nil {
			formatter.PrintError(fmt.Sprintf("failed to set key: %v", err))
			os.Exit(1)
		}

		// Handle response
		if !resp.Success {
			formatter.PrintError(resp.Message)
			os.Exit(1)
		}

		// Print success
		formatter.PrintSuccess(fmt.Sprintf("Set %s", key))
	},
}
