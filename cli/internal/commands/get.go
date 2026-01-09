package commands

import (
	"fmt"
	"os"

	"github.com/justnsmith/kv-storage-engine/cli/internal/client"
	"github.com/spf13/cobra"
)

var getCmd = &cobra.Command{
	Use:   "get <key>",
	Short: "Get the value of a key",
	Long: `Retrieve the value associated with a key from the storage engine.

Examples:
  kv get user:1
  kv get config:timeout
  kv get "my key with spaces"`,
	Args: cobra.ExactArgs(1),
	Run: func(cmd *cobra.Command, args []string) {
		key := args[0]

		// Create client
		c := client.NewClient(cfg.Server.Host, cfg.Server.Port, cfg.Server.GetTimeout())

		// Connect
		if err := c.Connect(); err != nil {
			formatter.PrintError(fmt.Sprintf("failed to connect: %v", err))
			os.Exit(1)
		}
		defer func() { _ = c.Close() }()

		// Execute GET
		resp, err := c.Get(key)
		if err != nil {
			formatter.PrintError(fmt.Sprintf("failed to get key: %v", err))
			os.Exit(1)
		}

		// Handle response
		if !resp.Success {
			if resp.Message == "NOT_FOUND" {
				formatter.PrintNotFound(key)
				os.Exit(1)
			}
			formatter.PrintError(resp.Message)
			os.Exit(1)
		}

		// Print value
		formatter.PrintValue(key, resp.Value)
	},
}
