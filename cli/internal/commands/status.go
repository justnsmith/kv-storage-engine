package commands

import (
	"fmt"
	"os"
	"time"

	"github.com/justnsmith/kv-store/cli/internal/client"
	"github.com/spf13/cobra"
)

var statusCmd = &cobra.Command{
	Use:   "status",
	Short: "Check server status and replication info",
	Long: `Query the server for status information including:
  - Server connectivity
  - Node ID and role (leader/follower)
  - Replication state
  - Active connections`,
	Run: func(cmd *cobra.Command, args []string) {
		timeout := time.Duration(cfg.Server.TimeoutMs) * time.Millisecond
		c := client.NewClient(cfg.Server.Host, cfg.Server.Port, timeout)

		if err := c.Connect(); err != nil {
			formatter.PrintError(fmt.Sprintf("Failed to connect: %v", err))
			os.Exit(1)
		}
		defer c.Close()

		resp, err := c.Status()
		if err != nil {
			formatter.PrintError(fmt.Sprintf("Failed to get status: %v", err))
			os.Exit(1)
		}

		if !resp.Success {
			formatter.PrintError(resp.Message)
			os.Exit(1)
		}

		formatter.PrintStatus(resp.Value)
	},
}

func init() {
	// No additional flags needed for status
}
