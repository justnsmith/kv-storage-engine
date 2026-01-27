package commands

import (
	"fmt"
	"os"

	"github.com/justnsmith/distributed-kv-storage/cli/internal/client"
	"github.com/spf13/cobra"
)

var deleteCmd = &cobra.Command{
	Use:   "delete <key>",
	Short: "Delete a key",
	Long: `Remove a key and its associated value from the storage engine.

Examples:
  kv delete user:1
  kv delete temp:cache
  kv delete "key with spaces"`,
	Args:    cobra.ExactArgs(1),
	Aliases: []string{"del", "rm"},
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

		// Execute DELETE
		resp, err := c.Delete(key)
		if err != nil {
			formatter.PrintError(fmt.Sprintf("failed to delete key: %v", err))
			os.Exit(1)
		}

		// Handle response
		if !resp.Success {
			formatter.PrintError(resp.Message)
			os.Exit(1)
		}

		// Print success
		formatter.PrintSuccess(fmt.Sprintf("Deleted %s", key))
	},
}
