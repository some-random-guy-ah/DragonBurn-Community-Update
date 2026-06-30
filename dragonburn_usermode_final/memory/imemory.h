#pragma once

class IMemory {
public:
    struct modules {
        std::uintptr_t client = 0;
        size_t client_size = 0;

        std::uintptr_t engine2 = 0;
        size_t engine2_size = 0;

        std::uintptr_t tier0 = 0;
        size_t tier0_size = 0;

        std::uintptr_t schemasystem = 0;
        size_t schemasystem_size = 0;

        std::uintptr_t vphysics2 = 0;
        size_t vphysics2_size = 0;
    };

    virtual ~IMemory() = default;

    virtual bool attach(const wchar_t* process_name) = 0;
    virtual void close() = 0;

    virtual bool      read_raw(uintptr_t address, void* buffer, size_t size) const = 0;
    virtual uintptr_t get_client_base() const = 0;
    virtual DWORD     get_pid()         const = 0;

    const modules& get_modules() const {
        return m_modules;
    }

    template <typename T>
    T read(uintptr_t address) const {
        T value{};
        read_raw(address, &value, sizeof(T));
        return value;
    }

    template <typename T = std::uintptr_t>
    T resolve_rip(std::uintptr_t address, std::int32_t offset = 3, std::int32_t length = 7)
    {
        const auto rva = this->read<std::int32_t>(address + offset);
        const auto resolved = address + length + rva;

        if constexpr (std::is_pointer_v<T>)
        {
            return reinterpret_cast<T>(resolved);
        }
        else
        {
            return static_cast<T>(resolved);
        }
    }

    std::uintptr_t find_pattern( std::uintptr_t module_base, std::string_view pattern ) const
    {
        if ( !module_base || pattern.empty( ) )
        {
            return 0;
        }

        const auto parsed = parse_pattern( pattern );
        if ( parsed.empty( ) )
        {
            return 0;
        }

        size_t module_size = 0;
        if (module_base == m_modules.client) module_size = m_modules.client_size;
        else if (module_base == m_modules.engine2) module_size = m_modules.engine2_size;
        else if (module_base == m_modules.schemasystem) module_size = m_modules.schemasystem_size;
        else if (module_base == m_modules.tier0) module_size = m_modules.tier0_size;
        else if (module_base == m_modules.vphysics2) module_size = m_modules.vphysics2_size;
        if (!module_size)
        {
            return 0;
        }

        const auto result = scan_chunked(module_base, module_size, parsed.size( ), [&]( const std::uint8_t* data, std::size_t size ) -> std::size_t
            {
                for ( std::size_t i = 0; i + parsed.size( ) <= size; ++i )
                {
                    auto match{ true };

                    for ( std::size_t j = 0; j < parsed.size( ); ++j )
                    {
                        if ( !parsed[ j ].m_wildcard && data[ i + j ] != parsed[ j ].m_value )
                        {
                            match = false;
                            break;
                        }
                    }

                    if ( match )
                    {
                        return i;
                    }
                }

                return std::size_t( -1 );
            } );

        return result.m_address;
    }

    std::uintptr_t find_vtable( std::uintptr_t module_base, std::string_view class_name) const
    {
	    const auto descriptor_name = ".?AV" + std::string( class_name ) + "@@";
	    std::uintptr_t type_descriptor{ 0 };

	    for ( const auto& sec : get_sections(module_base ))
	    {
		    constexpr auto required = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
		    if ( ( sec.m_characteristics & required ) != required || sec.m_size <= descriptor_name.size( ) )
		    {
			    continue;
		    }

		    const auto result = scan_chunked(sec.m_base, sec.m_size, descriptor_name.size( ), [ & ]( const std::uint8_t* data, std::size_t size ) -> std::size_t
			    {
				    for ( std::size_t i = 0; i + descriptor_name.size( ) < size; ++i )
				    {
					    if ( std::memcmp( data + i, descriptor_name.data( ), descriptor_name.size( ) + 1 ) == 0 )
					    {
						    return i;
					    }
				    }

				    return std::size_t( -1 );
			    } );

		    if ( result.m_found )
		    {
			    type_descriptor = result.m_address - 0x10;
			    break;
		    }
	    }

	    if ( !type_descriptor )
	    {
		    return 0;
	    }

	    const auto descriptor_rva = static_cast< std::uint32_t >( type_descriptor - module_base );
	    std::uintptr_t col_address{ 0 };

	    for ( const auto& sec : get_sections(module_base ))
	    {
		    if ( std::string_view{ sec.m_name }.find( ".rdata" ) == std::string_view::npos || sec.m_size < 0x30 )
		    {
			    continue;
		    }

		    const auto result = scan_chunked(sec.m_base, sec.m_size, 0x30, [ & ]( const std::uint8_t* data, std::size_t size ) -> std::size_t
			    {
				    for ( std::size_t i = 0; i + 0x30 <= size; i += 8 )
				    {
					    if ( reinterpret_cast< const std::uint32_t* >( data + i )[ 3 ] == descriptor_rva )
					    {
						    return i;
					    }
				    }

				    return std::size_t( -1 );
			    } );

		    if ( result.m_found )
		    {
			    col_address = result.m_address;
			    break;
		    }
	    }

	    if ( !col_address )
	    {
		    return 0;
	    }

	    const auto col_ref = this->find_qword_in_sections( module_base, col_address, IMAGE_SCN_MEM_READ );
	    return col_ref ? col_ref + 8 : 0;
    }

protected:
    modules m_modules{};

private:
    static constexpr std::size_t k_chunk_size{ 0x10000 };

    struct scan_result
    {
        std::uintptr_t m_address{};
        bool m_found{};
    };

    struct section_info
    {
        std::uintptr_t m_base{};
        std::size_t m_size{};
        std::uint32_t m_characteristics{};
        char m_name[ 9 ]{};
    };

    struct pattern_byte
    {
        std::uint8_t m_value{};
        bool m_wildcard{};
    };

    template <typename F>
    scan_result scan_chunked(std::uintptr_t base, std::size_t size, std::size_t overlap, F&& matcher) const
    {
        std::vector<std::uint8_t> buffer(k_chunk_size + overlap);

        for (std::size_t offset = 0; offset < size; offset += k_chunk_size)
        {
            const auto read_size = std::min( k_chunk_size + overlap,size - offset);
            if (!read_raw( base + offset, buffer.data(), read_size))
            {
                continue;
            }

            const auto match_offset = matcher(buffer.data(), read_size);
            if (match_offset != std::size_t(-1))
            {
                return {base + offset + match_offset, true};
            }
        }

        return {};
    }

    static std::vector<pattern_byte> parse_pattern( std::string_view pattern )
    {
        std::vector<pattern_byte> result;
        result.reserve( 64 );

        auto hex = [ ]( char c ) -> int
        {
            if ( c >= '0' && c <= '9' ) return c - '0';
            if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
            if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
            return -1;
        };

        for ( std::size_t i = 0; i < pattern.size( ); )
        {
            if ( pattern[ i ] == ' ' || pattern[ i ] == '\t' )
            {
                ++i;
                continue;
            }

            if ( pattern[ i ] == '?' )
            {
                result.push_back( { 0, true } );
                i += ( i + 1 < pattern.size( ) && pattern[ static_cast< std::size_t >( i ) + 1 ] == '?' ) ? 2 : 1;
                continue;
            }

            const auto high = hex( pattern[ i++ ] );
            if ( high < 0 )
            {
                continue;
            }

            const auto low = ( i < pattern.size( ) ) ? hex( pattern[ i ] ) : -1;
            if ( low >= 0 )
            {
                result.push_back( { static_cast< std::uint8_t >( ( high << 4 ) | low ), false } );
                ++i;
            }
            else
            {
                result.push_back( { static_cast< std::uint8_t >( high ), false } );
            }
        }

        return result;
    }

    std::vector<section_info> get_sections(std::uintptr_t module_base) const
    {
        std::vector<section_info> sections;

        const auto dos = read<IMAGE_DOS_HEADER>( module_base );
        if ( dos.e_magic != IMAGE_DOS_SIGNATURE )
        {
            return sections;
        }

        const auto nt = read<IMAGE_NT_HEADERS>( module_base + dos.e_lfanew );
        if ( nt.Signature != IMAGE_NT_SIGNATURE )
        {
            return sections;
        }

        const auto first = module_base + dos.e_lfanew + offsetof( IMAGE_NT_HEADERS, OptionalHeader ) + nt.FileHeader.SizeOfOptionalHeader;

        sections.reserve( nt.FileHeader.NumberOfSections );

        for ( std::uint16_t i = 0; i < nt.FileHeader.NumberOfSections; ++i )
        {
            const auto hdr = read<IMAGE_SECTION_HEADER>( first + i * sizeof( IMAGE_SECTION_HEADER ) );

            auto& sec = sections.emplace_back( );
            sec.m_base = module_base + hdr.VirtualAddress;
            sec.m_size = hdr.Misc.VirtualSize;
            sec.m_characteristics = hdr.Characteristics;
            std::memcpy( sec.m_name, hdr.Name, 8 );
            sec.m_name[ 8 ] = '\0';
        }

        return sections;
    }

    std::uintptr_t find_qword_in_sections( std::uintptr_t module_base, std::uintptr_t value, std::uint32_t section_filter ) const
    {
        for ( const auto& sec : get_sections(module_base ) )
        {
            if ( !( sec.m_characteristics & section_filter ) || sec.m_size < 8 )
            {
                continue;
            }

            const auto result = scan_chunked(sec.m_base, sec.m_size, 8, [ & ]( const std::uint8_t* data, std::size_t size ) -> std::size_t
                {
                    for ( std::size_t i = 0; i + 8 <= size; i += 8 )
                    {
                        if ( *reinterpret_cast< const std::uintptr_t* >( data + i ) == value )
                        {
                            return i;
                        }
                    }

                    return std::size_t( -1 );
                } );

            if ( result.m_found )
            {
                return result.m_address;
            }
        }

        return 0;
    }
};

inline std::unique_ptr<IMemory> g_memory;