package output

import (
	"encoding/json"
	"fmt"

	"github.com/fatih/color"
)

// Formatter handles output formatting
type Formatter struct {
	Format       string // "text" or "json"
	ColorEnabled bool
}

// NewFormatter creates a new formatter
func NewFormatter(format string, colorEnabled bool) *Formatter {
	return &Formatter{
		Format:       format,
		ColorEnabled: colorEnabled,
	}
}

// PrintSuccess prints a success message
func (f *Formatter) PrintSuccess(message string) {
	if f.Format == "json" {
		f.printJSON(map[string]interface{}{
			"success": true,
			"message": message,
		})
		return
	}

	if f.ColorEnabled {
		color.Green("✓ %s", message)
	} else {
		fmt.Println(message)
	}
}

// PrintValue prints a key-value result
func (f *Formatter) PrintValue(key, value string) {
	if f.Format == "json" {
		// Try to parse the value as JSON
		var parsedValue interface{}
		if err := json.Unmarshal([]byte(value), &parsedValue); err == nil {
			// Value is valid JSON, include it parsed
			f.printJSON(map[string]interface{}{
				"success": true,
				"key":     key,
				"value":   parsedValue,
			})
		} else {
			// Value is not JSON, include it as a string
			f.printJSON(map[string]interface{}{
				"success": true,
				"key":     key,
				"value":   value,
			})
		}
		return
	}

	// For text output, try to pretty-print JSON
	var parsedValue interface{}
	if err := json.Unmarshal([]byte(value), &parsedValue); err == nil {
		// Value is valid JSON, pretty print it
		prettyJSON, err := json.MarshalIndent(parsedValue, "", "  ")
		if err == nil {
			if f.ColorEnabled {
				color.Cyan("%s", string(prettyJSON))
			} else {
				fmt.Println(string(prettyJSON))
			}
			return
		}
	}

	// Not JSON or failed to parse, print as-is
	if f.ColorEnabled {
		color.Cyan("%s", value)
	} else {
		fmt.Println(value)
	}
}

// PrintError prints an error message
func (f *Formatter) PrintError(message string) {
	if f.Format == "json" {
		f.printJSON(map[string]interface{}{
			"success": false,
			"error":   message,
		})
		return
	}

	if f.ColorEnabled {
		color.Red("✗ Error: %s", message)
	} else {
		fmt.Printf("Error: %s\n", message)
	}
}

// PrintNotFound prints a not found message
func (f *Formatter) PrintNotFound(key string) {
	if f.Format == "json" {
		f.printJSON(map[string]interface{}{
			"success": false,
			"error":   "key not found",
			"key":     key,
		})
		return
	}

	if f.ColorEnabled {
		color.Yellow("Key not found: %s", key)
	} else {
		fmt.Printf("Key not found: %s\n", key)
	}
}

// PrintConfig prints configuration
func (f *Formatter) PrintConfig(cfg map[string]interface{}) {
	if f.Format == "json" {
		f.printJSON(cfg)
		return
	}

	fmt.Println("Current Configuration:")
	fmt.Println("---------------------")
	for k, v := range cfg {
		if f.ColorEnabled {
			color.Cyan("%s:", k)
			fmt.Printf("  %v\n", v)
		} else {
			fmt.Printf("%s: %v\n", k, v)
		}
	}
}

// printJSON prints data as JSON
func (f *Formatter) printJSON(data interface{}) {
	b, err := json.MarshalIndent(data, "", "  ")
	if err != nil {
		fmt.Printf("{\"error\": \"failed to marshal JSON: %s\"}\n", err)
		return
	}
	fmt.Println(string(b))
}

// PrintStatus formats and prints server status information
func (f *Formatter) PrintStatus(data string) {
	if f.Format == "json" {
		// For JSON, try to parse status as structured data
		// For now, just wrap in a success response
		f.printJSON(map[string]interface{}{
			"success": true,
			"status":  data,
		})
		return
	}

	// Text format - pretty print
	if f.ColorEnabled {
		color.Cyan("Server Status:")
		fmt.Println("---------------------")
	} else {
		fmt.Println("Server Status:")
		fmt.Println("---------------------")
	}

	fmt.Println(data)
}
