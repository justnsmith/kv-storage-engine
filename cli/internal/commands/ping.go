package commands

import (
	"fmt"
	"os"

	"github.com/justnsmith/kv-store/cli/internal/client"
	"github.com/spf13/cobra"
)

var pingCmd = &cobra.Command{
	Use:   "ping",
	Short: "Check server connectivity",
	Long: `Ping the storage engine server to verify connectivity.

This command is useful for:
- Checking if the server is running
- Verifying network connectivity
- Measuring basic response time

Examples:
  kv ping
  kv --host 10.0.0.5 ping`,
	Args: cobra.NoArgs,
	Run: func(cmd *cobra.Command, args []string) {
		// Create client
		c := client.NewClient(cfg.Server.Host, cfg.Server.Port, cfg.Server.GetTimeout())

		// Connect
		if err := c.Connect(); err != nil {
			formatter.PrintError(fmt.Sprintf("failed to connect: %v", err))
			os.Exit(1)
		}
		defer func() { _ = c.Close() }()

		// Execute PING
		resp, err := c.Ping()
		if err != nil {
			formatter.PrintError(fmt.Sprintf("ping failed: %v", err))
			os.Exit(1)
		}

		// Handle response
		if !resp.Success {
			formatter.PrintError(resp.Message)
			os.Exit(1)
		}

		// Print success
		formatter.PrintSuccess(fmt.Sprintf("PONG from %s:%d", cfg.Server.Host, cfg.Server.Port))
	},
}
