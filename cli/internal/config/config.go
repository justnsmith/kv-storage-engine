package config

import (
	"fmt"
	"os"
	"path/filepath"
	"time"

	"github.com/spf13/viper"
)

// Config holds all configuration values
type Config struct {
	Server ServerConfig `mapstructure:"server"`
	Output OutputConfig `mapstructure:"output"`
}

// ServerConfig holds server connection settings
type ServerConfig struct {
	Host      string `mapstructure:"host"`
	Port      int    `mapstructure:"port"`
	TimeoutMs int    `mapstructure:"timeout_ms"`
}

// OutputConfig holds output formatting settings
type OutputConfig struct {
	Format string `mapstructure:"format"`
	Color  bool   `mapstructure:"color"`
}

// Default values
const (
	DefaultHost      = "127.0.0.1"
	DefaultPort      = 9000
	DefaultTimeoutMs = 3000
	DefaultFormat    = "text"
	DefaultColor     = true
)

// Loads configuration from file and environment variables
func Load() (*Config, error) {
	v := viper.New()

	// Set defaults
	v.SetDefault("server.host", DefaultHost)
	v.SetDefault("server.port", DefaultPort)
	v.SetDefault("server.timeout_ms", DefaultTimeoutMs)
	v.SetDefault("output.format", DefaultFormat)
	v.SetDefault("output.color", DefaultColor)

	// Config file settings
	v.SetConfigName("config")
	v.SetConfigType("yaml")

	// Look in home directory first
	home, err := os.UserHomeDir()
	if err == nil {
		configPath := filepath.Join(home, ".kv")
		v.AddConfigPath(configPath)
	}

	// Also look in current directory
	v.AddConfigPath(".")

	// Read config file (ignore if not found)
	_ = v.ReadInConfig()

	// Environment variable overrides
	v.SetEnvPrefix("KV")
	v.AutomaticEnv()

	// Manual environment variable binding for nested config
	_ = v.BindEnv("server.host", "KV_HOST", "KV_SERVER_HOST")
	_ = v.BindEnv("server.port", "KV_PORT", "KV_SERVER_PORT")
	_ = v.BindEnv("server.timeout_ms", "KV_TIMEOUT_MS", "KV_SERVER_TIMEOUT_MS")
	_ = v.BindEnv("output.format", "KV_FORMAT", "KV_OUTPUT_FORMAT")
	_ = v.BindEnv("output.color", "KV_COLOR", "KV_OUTPUT_COLOR")

	// Unmarshal into struct
	cfg := &Config{}
	if err := v.Unmarshal(cfg); err != nil {
		return nil, fmt.Errorf("failed to unmarshal config: %w", err)
	}

	// Validate configuration
	if err := cfg.Validate(); err != nil {
		return nil, fmt.Errorf("invalid configuration: %w", err)
	}

	return cfg, nil
}

// Validate checks if the configuration is valid
func (c *Config) Validate() error {
	// Validate host
	if c.Server.Host == "" {
		return fmt.Errorf("server host cannot be empty")
	}

	// Validate port
	if c.Server.Port < 1 || c.Server.Port > 65535 {
		return fmt.Errorf("server port must be between 1 and 65535, got %d", c.Server.Port)
	}

	// Validate timeout
	if c.Server.TimeoutMs < 0 {
		return fmt.Errorf("server timeout_ms cannot be negative")
	}

	// Validate format
	if c.Output.Format != "text" && c.Output.Format != "json" {
		return fmt.Errorf("output format must be 'text' or 'json', got '%s'", c.Output.Format)
	}

	return nil
}

// GetTimeout returns the timeout as a duration
func (s *ServerConfig) GetTimeout() time.Duration {
	return time.Duration(s.TimeoutMs) * time.Millisecond
}

// CreateDefaultConfig creates a default config file in ~/.kv/config.yaml
func CreateDefaultConfig() error {
	home, err := os.UserHomeDir()
	if err != nil {
		return fmt.Errorf("failed to get home directory: %w", err)
	}

	configDir := filepath.Join(home, ".kv")
	configPath := filepath.Join(configDir, "config.yaml")

	// Create directory if it doesn't exist
	if err := os.MkdirAll(configDir, 0755); err != nil {
		return fmt.Errorf("failed to create config directory: %w", err)
	}

	// Don't overwrite existing config
	if _, err := os.Stat(configPath); err == nil {
		return fmt.Errorf("config file already exists at %s", configPath)
	}

	// Write default config
	defaultConfig := `# KV Storage Engine CLI Configuration

server:
  # Server connection settings
  host: 127.0.0.1
  port: 9000
  timeout_ms: 3000

output:
  # Output format: text or json
  format: text

  # Enable colored output (only applies to text format)
  color: true
`

	if err := os.WriteFile(configPath, []byte(defaultConfig), 0644); err != nil {
		return fmt.Errorf("failed to write config file: %w", err)
	}

	return nil
}

// GetConfigPath returns the path to the config file if it exists
func GetConfigPath() (string, error) {
	home, err := os.UserHomeDir()
	if err != nil {
		return "", err
	}

	configPath := filepath.Join(home, ".kv", "config.yaml")

	if _, err := os.Stat(configPath); err == nil {
		return configPath, nil
	}

	// Check current directory
	if _, err := os.Stat("config.yaml"); err == nil {
		return "config.yaml", nil
	}

	return "", fmt.Errorf("config file not found")
}
