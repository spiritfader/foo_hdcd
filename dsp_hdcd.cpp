#define MYVERSION "1.20"

/*
   Copyright (C) 2010-2017, Christopher Snowhill,
   All rights reserved.                          

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote 
        products derived from this software without specific prior written 
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*

	change log

2022-02-05 06:17 UTC - kode54
- Redid the gain stepping algorithm again
- Added patron credits
- Version is now 1.20

2017-02-04 02:12 UTC - kode54
- Add link to about string
- Version is now 1.19

2017-01-21 21:36 UTC - kode54
- Added a metadata trigger to disable processing for a given track. Just set the
  meta field "HDCD" to "no".
- Version is now 1.18

2016-08-06 20:44 UTC - kode54
- Reinstated support for arbitrary channel counts
- Added seekback of 10 seconds to preprocessor, hopefully to pick up the control
  when seeking around
- Added a check in get_dynamic_info for calls which occur before the decoder is
  initialized
- Cleaned up some warnings
- Version is now 1.17

2016-08-06 02:42 UTC - kode54
- Updated to latest HDCD processor with updates from FFmpeg, thanks to Burt P.
- Version is now 1.16

2012-07-18 03:37 UTC - kode54
- HDCD scanning now sustains processing before applying levels to output
- Maintains peak extension flag for the entire track
- Starts processing from the beginning with the volume level flags from
  post-sustain status, to prevent sharp volume level transitions on track
  change
- Version is now 1.15

2011-12-10 01:39 UTC - kode54
- Further optimizations from Gumboot
- Version is now 1.14

2011-12-10 00:38 UTC - kode54
- Fixed sustain counter so decoder won't process non-HDCD content
- Version is now 1.13

2011-12-08 23:56 UTC - kode54
- Implemented optimized C decoder by Gumboot
- Version is now 1.12

2011-02-22 14:14 UTC - kode54
- Changed advanced configuration option to a radio selection, defaulting to only
  halving the volume if peak extension is enabled
- Version is now 1.11

2011-02-05 10:40 UTC - kode54
- Added advanced configuration option to negate volume halving normally applied
  by HDCD decoding
- Version is now 1.10

2011-02-04 17:21 UTC - kode54
- Added full feature detection to HDCD scanner
- Version is now 1.9

2011-02-04 15:49 UTC - kode54
- Increased HDCD packet life to 10 seconds
- Added more comments to HDCD decoder

2011-01-25 18:28 UTC - kode54
- Removed sort header setting from HDCD tester results dialog
- Improved HDCD tester to accept all files and only skip unacceptable files when
  invoking the scanner
- Version is now 1.8

2011-01-17 11:01 UTC - kode54
- HDCD postprocessor service will only start outputting decoded data if the HDCD
  signal continues for a full five seconds
- Version is now 1.7

2011-01-17 08:45 UTC - kode54
- HDCD info reporter now removes HDCD fields if HDCD times out and turns off
- Version is now 1.6

2010-07-31 01:00 UTC - kode54
- Added real-time HDCD feature reporting
- Version is now 1.5

2010-05-24 14:52 UTC - kode54
- Changed HDCD processor to give up after 5 seconds of no HDCD signatures
- Version is now 1.4

2010-05-22 04:57 UTC - kode54
- Implemented new decode_postprocessor interface, rendering the DSP obsolete

2010-04-09 09:44 UTC - kode54
- Changed DSP to buffer whole audio_chunks, at least one second or one chunk worth
- Version is now 1.3

2010-03-14 14:50 UTC - kode54
- Changed the HDCD decoder peak extension condition to a single if statement
- Version is now 1.2

2010-03-14 14:16 UTC - kode54
- Cleaned up a few things.
- Added HDCD detection indicator to console
- Version is now 1.1

2010-03-14 13:40 UTC - kode54
- Initial release

*/

#include <foobar2000/SDK/foobar2000.h>

#include "hdcd_decode2.h"

static const GUID guid_cfg_parent_hdcd = { 0x1067a697, 0x31ab, 0x416a, { 0x9e, 0x94, 0x56, 0xbc, 0x6e, 0x68, 0xda, 0xad } };
static const GUID guid_cfg_parent_amp = { 0xdf059409, 0xa5a3, 0x4155, { 0x9e, 0x9d, 0x13, 0x89, 0x61, 0x9b, 0x43, 0xcb } };
static const GUID guid_cfg_hdcd_amp_never = { 0x590aa4fd, 0x26da, 0x4065, { 0x9f, 0xcb, 0xaf, 0xfc, 0xd1, 0x4, 0x27, 0xfd } };
static const GUID guid_cfg_hdcd_amp_not_peak = { 0x9565b81f, 0x82d5, 0x4656, { 0xa0, 0x4, 0xbe, 0xc5, 0xd1, 0x7d, 0x2a, 0xe1 } };
static const GUID guid_cfg_hdcd_amp_always = { 0xd4b8551, 0xa744, 0x49e0, { 0xa3, 0xa7, 0x7f, 0x25, 0xf1, 0xdd, 0xb, 0xf9 } };

advconfig_branch_factory cfg_hdcd_parent("HDCD decoder", guid_cfg_parent_hdcd, advconfig_branch::guid_branch_decoding, 0);
advconfig_branch_factory cfg_hdcd_amp_parent("Halve output volume", guid_cfg_parent_amp, guid_cfg_parent_hdcd, 0);

advconfig_radio_factory cfg_hdcd_amp_never("Never", guid_cfg_hdcd_amp_never, guid_cfg_parent_amp, 0, false);
advconfig_radio_factory cfg_hdcd_amp_not_peak("Only if peak extension is enabled", guid_cfg_hdcd_amp_not_peak, guid_cfg_parent_amp, 0, true);
advconfig_radio_factory cfg_hdcd_amp_always("Always", guid_cfg_hdcd_amp_always, guid_cfg_parent_amp, 0, false);

class hdcd_dsp : public dsp_impl_base {
	pfc::array_t<hdcd_state_t> decoders;
	hdcd_state_stereo_t decoder;

	dsp_chunk_list_impl original_chunks;
	pfc::array_t<t_int32> buffer;

	unsigned amp;

	unsigned srate, nch, channel_config;

	bool gave_up, info_emitted, peak_extension;

	void init()
	{
		if (nch != 2)
		{
			decoders.set_size(nch);
			for (unsigned i = 0; i < nch; i++)
				hdcd_reset(&decoders[i], srate);
		}
		else
			hdcd_reset_stereo(&decoder, srate);
	}

	void cleanup()
	{
		decoders.set_size(0);
		original_chunks.remove_all();
		buffer.set_size( 0 );
		srate = 0;
		nch = 0;
		channel_config = 0;
		gave_up = false;
		info_emitted = false;
		peak_extension = false;
	}

	void flush_chunk()
	{
		if ( original_chunks.get_count() )
		{
			unsigned enabled;
			if (nch == 2)
				enabled = decoder.channel[0].sustain && decoder.channel[1].sustain;
			else
			{
				enabled = 1;
				for (unsigned i = 0; i < nch; i++)
					enabled = enabled && decoders[i].sustain;
			}

			for ( unsigned i = 0; i < original_chunks.get_count(); i++ )
			{
				audio_chunk * in_chunk = original_chunks.get_item( i );
				audio_chunk * out_chunk = insert_chunk( in_chunk->get_data_size() );
				out_chunk->copy( *in_chunk );
				if ( enabled ) process_chunk( out_chunk );
			}
		}
		cleanup();
	}

	void process_chunk( audio_chunk * chunk )
	{
		if (nch == 2)
			peak_extension = !!(decoder.channel[0].control & decoder.channel[1].control & 16);
		else
		{
			peak_extension = 1;
			for (unsigned i = 0; i < nch; ++i)
				peak_extension = peak_extension && !!(decoders[i].control & 16);
		}
		bool amp = this->amp == 0 ? true : this->amp == 2 ? false : !peak_extension;
		const audio_sample target_scale = amp ? 2.0f : 1.0f;
		int data = chunk->get_sample_count() * nch;
		buffer.grow_size( data );
		audio_math::convert_to_int32( chunk->get_data(), data, buffer.get_ptr(), 1. / 65536. );

		if (nch == 2)
			hdcd_process_stereo(&decoder, buffer.get_ptr(), chunk->get_sample_count());
		else
		{
			for (unsigned i = 0; i < nch; ++i)
				hdcd_process(&decoders[i], buffer.get_ptr() + i, chunk->get_sample_count(), nch);
		}

		audio_math::convert_from_int32( buffer.get_ptr(), data, chunk->get_data(), target_scale );
	}

public:
	hdcd_dsp()
	{
		amp = cfg_hdcd_amp_never.get() ? 0 : cfg_hdcd_amp_not_peak.get() ? 1 : cfg_hdcd_amp_always.get() ? 2 : 0;
		cleanup();
	}

	~hdcd_dsp()
	{
		cleanup();
	}

	static GUID g_get_guid()
	{
		static const GUID guid = { 0xaef54f, 0x54f6, 0x4f97, { 0x83, 0x33, 0x67, 0x40, 0x94, 0x9d, 0x38, 0xbf } };
		return guid;
	}

	static void g_get_name(pfc::string_base &p_out)
	{
		p_out = "HDCD decoder";
	}

	virtual void on_endoftrack(abort_callback &p_abort)
	{
		flush_chunk();
	}

	virtual void on_endofplayback(abort_callback &p_abort)
	{
		flush_chunk();
	}

	virtual bool on_chunk(audio_chunk *chunk, abort_callback &p_abort)
	{
		if ( gave_up ) return true;

        metadb_handle_ptr fh;
        if ( !get_cur_file( fh ) )
		{
			flush_chunk();
            return true;
        }

        file_info_impl i;
        if ( !fh->get_info_async( i ) )
		{
			flush_chunk();
            return true;
        }

        if ( i.info_get_decoded_bps() != 16 )
		{
			flush_chunk();
            return true;
        }

		const char * encoding = i.info_get( "encoding" );
		if ( !encoding || pfc::stricmp_ascii( encoding, "lossless" ) )
		{
			flush_chunk();
			return true;
		}

		if ( srate != chunk->get_sample_rate() || nch != chunk->get_channels() || channel_config != chunk->get_channel_config() )
		{
			flush_chunk();
			srate = chunk->get_sample_rate();
			nch = chunk->get_channels();
			channel_config = chunk->get_channel_config();
			init();
		}

		unsigned enabled;
		if (nch == 2)
			enabled = decoder.channel[0].sustain && decoder.channel[1].sustain;
		else
		{
			enabled = 1;
			for (unsigned i = 0; i < nch; i++)
				enabled = enabled && decoders[i].sustain;
		}

		if ( enabled )
		{
			if ( !info_emitted )
			{
				console::print( "HDCD detected." );
				info_emitted = true;
			}

			if ( original_chunks.get_count() )
			{
				for ( unsigned i = 0; i < original_chunks.get_count(); i++ )
				{
					audio_chunk * in_chunk = original_chunks.get_item( i );
					audio_chunk * out_chunk = insert_chunk( in_chunk->get_data_size() );
					out_chunk->copy( *in_chunk );
					process_chunk( out_chunk );
				}
				original_chunks.remove_all();
			}

			process_chunk( chunk );

			return true;
		}

		original_chunks.add_chunk( chunk );

		process_chunk( chunk );

		if ( original_chunks.get_duration() >= 10.0 && original_chunks.get_count() > 1 )
		{
			flush_chunk();
			gave_up = true;
			return true;
		}

		return false;
	}

	virtual void flush()
	{
		cleanup();
	}

	virtual double get_latency()
	{
		double latency = 0;
		if ( original_chunks.get_count() )
		{
			latency += original_chunks.get_duration();
		}
		return latency;
	}

	virtual bool need_track_change_mark()
	{
		return false;
	}
};

class hdcd_postprocessor_instance : public decode_postprocessor_instance
{
	pfc::array_t<hdcd_state_t> decoders;
	hdcd_state_stereo_t decoder;

	dsp_chunk_list_impl original_chunks;
	pfc::array_t<t_int32> buffer;

	unsigned amp;

	unsigned srate, nch, channel_config;

	bool gave_up, sustained, peak_extension;

	void init()
	{
		if (nch != 2)
		{
			decoders.set_size(nch);
			for (unsigned i = 0; i < nch; i++)
				hdcd_reset(&decoders[i], srate);
		}
		else
			hdcd_reset_stereo(&decoder, srate);
	}

	void cleanup()
	{
		decoders.set_size( 0 );
		original_chunks.remove_all();
		buffer.set_size( 0 );
		srate = 0;
		nch = 0;
		channel_config = 0;
		gave_up = false;
		sustained = false;
		peak_extension = false;
	}

	unsigned flush_chunks( dsp_chunk_list & p_chunk_list, unsigned insert_point )
	{
		unsigned ret = 0;

		if ( original_chunks.get_count() )
		{
			ret = original_chunks.get_count();

			unsigned enabled;
			if (nch == 2)
				enabled = decoder.channel[0].sustain && decoder.channel[1].sustain;
			else
			{
				enabled = 1;
				for ( unsigned i = 0; i < nch; i++ )
					enabled = enabled && decoders[ i ].sustain;
			}

			for ( unsigned i = 0; i < ret; i++ )
			{
				audio_chunk * in_chunk = original_chunks.get_item( i );
				audio_chunk * out_chunk = p_chunk_list.insert_item( insert_point++, in_chunk->get_data_size() );
				out_chunk->copy( *in_chunk );
				if ( enabled ) process_chunk( out_chunk );
			}

			original_chunks.remove_all();
		}

		return ret;
	}

	void process_chunk( audio_chunk * chunk )
	{
		if (nch == 2)
			peak_extension = !!(decoder.channel[0].control & decoder.channel[1].control & 16);
		else
		{
			peak_extension = 1;
			for (unsigned i = 0; i < nch; ++i)
				peak_extension = peak_extension && !!(decoders[i].control & 16);
		}
		bool amp = this->amp == 0 ? true : this->amp == 2 ? false : !peak_extension;
		const audio_sample target_scale = amp ? 2.0f : 1.0f;

		int data = chunk->get_sample_count() * nch;
		buffer.grow_size( data );
		audio_math::convert_to_int32( chunk->get_data(), data, buffer.get_ptr(), 1. / 65536. );

		if (nch == 2)
			hdcd_process_stereo(&decoder, buffer.get_ptr(), chunk->get_sample_count());
		else
			for (unsigned i = 0; i < nch; ++i)
				hdcd_process(&decoders[i], buffer.get_ptr() + i, chunk->get_sample_count(), nch);

		audio_math::convert_from_int32( buffer.get_ptr(), data, chunk->get_data(), target_scale );
	}

public:
	hdcd_postprocessor_instance()
	{
		amp = cfg_hdcd_amp_never.get() ? 0 : cfg_hdcd_amp_not_peak.get() ? 1 : cfg_hdcd_amp_always.get() ? 2 : 0;
		cleanup();
	}

	~hdcd_postprocessor_instance()
	{
		cleanup();
	}

	virtual bool run( dsp_chunk_list & p_chunk_list, t_uint32 p_flags, abort_callback & p_abort )
	{
		if ( gave_up || p_flags & flag_altered ) return false;

		bool modified = false;

		for ( unsigned i = 0; i < p_chunk_list.get_count(); )
		{
			audio_chunk * chunk = p_chunk_list.get_item( i );

			if ( srate != chunk->get_sample_rate() || nch != chunk->get_channels() || channel_config != chunk->get_channel_config() )
			{
				i += flush_chunks( p_chunk_list, i );
				srate = chunk->get_sample_rate();
				nch = chunk->get_channels();
				channel_config = chunk->get_channel_config();
				init();
			}

			unsigned enabled;
			if (nch == 2)
				enabled = decoder.channel[0].sustain && decoder.channel[1].sustain;
			else
			{
				enabled = 1;
				for (unsigned j = 0; j < nch; j++)
					enabled = enabled && decoders[j].sustain;
			}

			if ( enabled && sustained )
			{
				i += flush_chunks( p_chunk_list, i );
				process_chunk( chunk );
				modified = true;
				i++;
				continue;
			}

			original_chunks.add_chunk( chunk );

			process_chunk( chunk );

			p_chunk_list.remove_by_idx( i );

			if ( original_chunks.get_duration() >= 10.0 && original_chunks.get_count() > 1 )
			{
				if ( !enabled )
				{
					flush_chunks( p_chunk_list, i );
					cleanup();
					gave_up = true;
					break;
				}
				else sustained = true;
			}
		}

		if ( p_flags & flag_eof )
		{
			flush_chunks( p_chunk_list, p_chunk_list.get_count() );
			cleanup();
		}

		return modified;
	}

	virtual bool get_dynamic_info( file_info & p_out )
	{
		if (!nch) return false;

		unsigned enabled;
		if (nch == 2)
			enabled = decoder.channel[0].sustain && decoder.channel[1].sustain;
		else
		{
			enabled = 1;
			for (unsigned i = 0; i < nch; i++)
				enabled = enabled && decoders[i].sustain;
		}

		if ( enabled )
		{
			unsigned status = nch == 2 ? decoder.channel[0].control : decoders[0].control;

			p_out.info_set_int( "bitspersample", 24 );
			p_out.info_set( "hdcd", "yes" );

			p_out.info_set( "hdcd_peak_extend", ( status & 16 ) ? "yes" : "no" );
			p_out.info_set( "hdcd_transient_filter", ( status & 32 ) ? "yes" : "no" );

			unsigned gain = status & 15;
			pfc::string8 temp;
			if ( gain ) temp.add_byte( '-' );
			temp.add_byte( '0' + ( gain / 2 ) );
			temp.add_byte( '.' );
			temp.add_byte( '0' + ( gain & 1 ) * 5 );
			temp += " dB";
			p_out.info_set( "hdcd_gain", temp );

			return true;
		}
		else if ( p_out.info_get( "hdcd" ) )
		{
			p_out.info_set_int( "bitspersample", 16 );

			p_out.info_remove( "hdcd" );
			p_out.info_remove( "hdcd_peak_extend" );
			p_out.info_remove( "hdcd_transient_filter" );

			return true;
		}
		return false;
	}

	virtual void flush()
	{
		cleanup();
	}

	virtual double get_buffer_ahead()
	{
		return 10.0;
	}
};

class hdcd_postprocessor_entry : public decode_postprocessor_entry
{
public:
	virtual bool instantiate( const file_info & info, decode_postprocessor_instance::ptr & out )
	{
        if ( info.info_get_decoded_bps() != 16)
		{
            return false;
        }

		const char * encoding = info.info_get( "encoding" );
		if ( !encoding || pfc::stricmp_ascii( encoding, "lossless" ) )
		{
			return false;
		}

		const char * hdcd = info.meta_get( "hdcd", 0 );
		if ( hdcd && pfc::stricmp_ascii( hdcd, "no" ) == 0 )
		{
			return false;
		}

		out = new service_impl_t< hdcd_postprocessor_instance >;

		return true;
	}
};

//static dsp_factory_nopreset_t  <hdcd_dsp>                 g_hdcd_dsp_factory;
static service_factory_single_t<hdcd_postprocessor_entry> g_hdcd_postprocessor_entry_factory;


static const char about_string[] = "HDCD is a registered trademark of Microsoft Corporation.\n\nhttps://www.patreon.com/kode54\n\n";

DECLARE_COMPONENT_VERSION("HDCD decoder", MYVERSION, about_string);

VALIDATE_COMPONENT_FILENAME("foo_hdcd.dll");
